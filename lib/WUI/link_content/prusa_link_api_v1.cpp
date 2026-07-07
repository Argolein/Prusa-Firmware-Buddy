#include "prusa_link_api_v1.h"
#include "basic_gets.h"
#include "../nhttp/file_info.h"
#include "../nhttp/file_command.h"
#include "../nhttp/headers.h"
#include "../nhttp/gcode_upload.h"
#include "../nhttp/job_command.h"
#include "../nhttp/send_json.h"
#include "../nhttp/status_renderer.h"
#include "../nhttp/filament_renderer.h"
#include "../nhttp/filament_command.h"
#include "../nhttp/mesh_renderer.h"
#include "../nhttp/tool_mapping_renderer.h"
#include "../nhttp/tool_mapping_command.h"
#include "../wui_api.h"
#include "prusa_api_helpers.hpp"

#include <tool_index.hpp>

#include <marlin_client.hpp>
#include <common/path_utils.h>
#include <transfers/monitor.hpp>
#include <transfers/changed_path.hpp>
#include <state/printer_state.hpp>
#include <option/has_side_leds.h>
#if HAS_SIDE_LEDS()
    #include <leds/side_strip_handler.hpp>
#endif
#include <option/has_tool_mapping.h>
#if HAS_TOOL_MAPPING()
    #include <marlin_vars.hpp>
    #include <fsm_states.hpp>
    #include <client_response.hpp>
    #include <fsm/print_preview_phases.hpp>
#endif
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <charconv>
#include <unistd.h>

namespace nhttp::link_content {

using http::Method;
using http::Status;
using std::nullopt;
using std::optional;
using std::string_view;
using namespace handler;
using namespace transfers;
using nhttp::printer::FilamentCommand;
using nhttp::printer::FileCommand;
using nhttp::printer::FileInfo;
using nhttp::printer::GcodeUpload;
using nhttp::printer::JobCommand;
using nhttp::printer::ToolMappingCommand;
using printer_state::DeviceState;
using transfers::ChangedPath;

namespace {
    optional<int> get_job_id(string_view str) {
        int ret;
        auto result = from_chars_light(str.begin(), str.end(), ret);
        if (result.ec != std::errc {}) {
            return nullopt;
        }

        return ret;
    }

    void stop_print(const RequestParser &parser, handler::Step &out) {
        switch (printer_state::get_state(false)) {
        case DeviceState::Printing:
        case DeviceState::Paused:
        case DeviceState::Attention:
            marlin_client::print_abort();
            out.next = StatusPage(Status::NoContent, parser);
            return;
        default:
            out.next = StatusPage(Status::Conflict, parser);
            return;
        }
    }

    void pause_print(const RequestParser &parser, handler::Step &out) {
        if (printer_state::get_state(false) == DeviceState::Printing) {
            marlin_client::print_pause();
            out.next = StatusPage(Status::NoContent, parser);
        } else {
            out.next = StatusPage(Status::Conflict, parser);
        }
    }

    void resume_print(const RequestParser &parser, handler::Step &out) {
        if (printer_state::get_state(false) == DeviceState::Paused) {
            marlin_client::print_resume();
            out.next = StatusPage(Status::NoContent, parser);
        } else {
            out.next = StatusPage(Status::Conflict, parser);
        }
    }

    void handle_command(string_view command, const RequestParser &parser, handler::Step &out) {
        if (command == "resume") {
            resume_print(parser, out);
        } else if (command == "pause") {
            pause_print(parser, out);
        } else {
            out.next = StatusPage(Status::BadRequest, parser);
        }
    }

    void start_bed_leveling(const RequestParser &parser, handler::Step &out) {
        // Mirror the printer's Mesh Bed Leveling menu item (MI_MESH_BED): only
        // start when the printer is idle and nothing is queued, then home (if
        // needed) and probe. G29 auto-expands to P1 / P3.2 / P3.13 / A.
        const DeviceState state = printer_state::get_state(false);
        const bool ready = state == DeviceState::Idle || state == DeviceState::Ready
            || state == DeviceState::Finished || state == DeviceState::Stopped;
        if (!ready || marlin_vars().gqueue != 0) {
            out.next = StatusPage(Status::Conflict, parser);
            return;
        }
        marlin_client::gcode("G28 O");
        marlin_client::gcode("G29");
        out.next = StatusPage(Status::Accepted, parser);
    }

    // Argo: interactive PrusaLink controls. The web UI sends the requested value
    // in the URL path; we clamp server-side (the client limits are UX only) and
    // set the target without waiting, matching Prusa Connect's behaviour (allowed
    // even mid-print; the running G-code may later override it).
    constexpr int max_nozzle_target = 295;
    constexpr int max_bed_target = 115;

    void set_nozzle_target(int value, const RequestParser &parser, handler::Step &out) {
        value = std::clamp(value, 0, max_nozzle_target);
        for (auto tool : PhysicalToolIndex::all()) {
            marlin_client::set_target_nozzle(static_cast<int16_t>(value), tool);
        }
        out.next = StatusPage(Status::NoContent, parser);
    }

    void set_bed_target(int value, const RequestParser &parser, handler::Step &out) {
        value = std::clamp(value, 0, max_bed_target);
        marlin_client::set_target_bed(static_cast<int16_t>(value));
        out.next = StatusPage(Status::NoContent, parser);
    }

#if HAS_SIDE_LEDS()
    void set_chamber_light(bool on, const RequestParser &parser, handler::Step &out) {
        auto &strip = leds::SideStripHandler::instance();
        // "Off" is persisted as max_brightness == 0 (the same config the on-screen
        // "Chamber Lights" menu writes), so it survives a reboot. We keep a RAM
        // shadow of the last on-brightness to restore on switch-on; after a
        // reboot-while-off the shadow is empty and we fall back to full.
        static uint8_t saved_brightness = 0;
        if (on) {
            if (strip.get_max_brightness() == 0) {
                strip.set_max_brightness(saved_brightness > 0 ? saved_brightness : 255);
            }
        } else {
            if (const uint8_t current = strip.get_max_brightness(); current > 0) {
                saved_brightness = current;
            }
            strip.set_max_brightness(0);
        }
        out.next = StatusPage(Status::NoContent, parser);
    }
#endif

#if HAS_TOOL_MAPPING()
    // Argo: web tool mapping — send the tools-mapping preview screen's Print/Abort
    // response, but only while the print-preview FSM is actually holding there
    // (mirrors Connect's dialog_action guard). The mapping itself is set first via
    // PUT /api/v1/mapping.
    void mapping_response(Response response, const RequestParser &parser, handler::Step &out) {
        std::optional<fsm::States::Top> top;
        marlin_vars().peek_fsm_states([&](const fsm::States &states) {
            top = states.get_top();
        });
        if (!top.has_value()
            || top->fsm_type != ClientFSM::PrintPreview
            || GetEnumFromPhaseIndex<PhasesPrintPreview>(top->data.GetPhase()) != PhasesPrintPreview::tools_mapping) {
            out.next = StatusPage(Status::Conflict, parser);
            return;
        }
        marlin_client::FSM_response(PhasesPrintPreview::tools_mapping, response);
        out.next = StatusPage(Status::NoContent, parser);
    }
#endif
} // namespace

Selector::Accepted PrusaLinkApiV1::accept(const RequestParser &parser, handler::Step &out) const {
    // This is a little bit of a hack (similar one is in Connect). We want to
    // watch as often as possible if the USB is plugged in or not, to
    // invalidate dir listing caches in the browser. As we don't really have a
    // better place, we place it here.
    ChangedPath::instance.media_inserted(wui_media_inserted());

    const string_view uri = parser.uri();

    // Claim the whole /api/v1 prefix.
    const auto suffix_opt = remove_prefix(uri, "/api/v1/");
    if (!suffix_opt.has_value()) {
        return Accepted::NextSelector;
    }

    const auto suffix = *suffix_opt;

    if (!parser.check_auth(out)) {
        return Accepted::Accepted;
    }

    if (suffix == "storage") {
        get_only(SendJson(EmptyRenderer(get_storage), parser.can_keep_alive()), parser, out);
        return Accepted::Accepted;
    } else if (suffix == "info") {
        get_only(SendJson(EmptyRenderer(get_info), parser.can_keep_alive()), parser, out);
        return Accepted::Accepted;
    } else if (auto job_suffix_opt = remove_prefix(suffix, "job/"); job_suffix_opt.has_value()) {
        auto job_suffix = *job_suffix_opt;
        auto id = get_job_id(job_suffix);
        if (!id.has_value()) {
            out.next = StatusPage(Status::BadRequest, parser);
            return Accepted::Accepted;
        }
        if (id.value() != marlin_vars().job_id) {
            out.next = StatusPage(Status::NotFound, parser);
            return Accepted::Accepted;
        }
        switch (parser.method) {
        case Method::Put: {
            auto slash = job_suffix.find('/');
            if (slash == string_view::npos) {
                out.next = StatusPage(Status::BadRequest, parser);
            } else {
                auto command_view = string_view(job_suffix.begin() + job_suffix.find('/') + 1, job_suffix.end());
                handle_command(command_view, parser, out);
            }
            return Accepted::Accepted;
        }
        case Method::Delete:
            stop_print(parser, out);
            return Accepted::Accepted;
        default:
            out.next = StatusPage(Status::MethodNotAllowed, parser);
            return Accepted::Accepted;
        }
    } else if (suffix == "job") {
        if (printer_state::has_job()) {
            get_only(SendJson(EmptyRenderer(get_job_v1), parser.can_keep_alive()), parser, out);
            return Accepted::Accepted;
        } else {
            out.next = StatusPage(Status::NoContent, parser);
            return Accepted::Accepted;
        }
    } else if (suffix == "status") {
        auto status = Monitor::instance.status();
        optional<TransferId> id;
        if (status.has_value()) {
            id = status->id;
        } else {
            id = nullopt;
        }
        get_only(SendJson(StatusRenderer(id), parser.can_keep_alive()), parser, out);
        return Accepted::Accepted;
    } else if (suffix == "transfer") {
        if (auto status = Monitor::instance.status(); status.has_value()) {
            get_only(SendJson(TransferRenderer(status->id, http::APIVersion::v1), parser.can_keep_alive()), parser, out);
            return Accepted::Accepted;
        } else {
            out.next = StatusPage(Status::NoContent, parser);
            return Accepted::Accepted;
        }
    } else if (suffix == "filament") {
        // Per-tool filament type + color (Filament Color Manager).
        get_only(SendJson(FilamentRenderer(), parser.can_keep_alive()), parser, out);
        return Accepted::Accepted;
    } else if (suffix == "mesh") {
        // GET reads the mesh grid; POST triggers bed leveling (re)creating it.
        if (parser.method == Method::Post) {
            start_bed_leveling(parser, out);
        } else {
            get_only(SendJson(MeshRenderer(), parser.can_keep_alive()), parser, out);
        }
        return Accepted::Accepted;
    } else if (suffix == "mapping") {
        // Argo: web tool mapping — GET reads the current G-code-filament ↔ tool
        // mapping (only meaningful while held at the tools-mapping preview phase);
        // PUT applies a new mapping.
#if HAS_TOOL_MAPPING()
        if (parser.method == Method::Put) {
            if (!parser.content_length.has_value()) {
                out.next = StatusPage(Status::LengthRequired, parser);
            } else {
                out.next = ToolMappingCommand(*parser.content_length, parser.can_keep_alive(), parser.accepts_json);
            }
        } else {
            get_only(SendJson(ToolMappingRenderer(), parser.can_keep_alive()), parser, out);
        }
#else
        out.next = StatusPage(Status::NotFound, parser);
#endif
        return Accepted::Accepted;
    } else if (auto mapping_suffix_opt = remove_prefix(suffix, "mapping/"); mapping_suffix_opt.has_value()) {
        // Argo: confirm (Print) / cancel (Abort) the tools-mapping preview screen.
#if HAS_TOOL_MAPPING()
        if (parser.method != Method::Post) {
            out.next = StatusPage(Status::MethodNotAllowed, parser);
        } else if (const auto action = *mapping_suffix_opt; action == "confirm") {
            mapping_response(Response::Print, parser, out);
        } else if (action == "cancel") {
            mapping_response(Response::Abort, parser, out);
        } else {
            out.next = StatusPage(Status::NotFound, parser);
        }
#else
        out.next = StatusPage(Status::NotFound, parser);
#endif
        return Accepted::Accepted;
    } else if (auto tool_suffix_opt = remove_prefix(suffix, "filament/"); tool_suffix_opt.has_value()) {
        int tool = -1;
        const auto r = from_chars_light(tool_suffix_opt->begin(), tool_suffix_opt->end(), tool);
        if (r.ec != std::errc {} || tool < 0 || tool >= PhysicalToolIndex::count) {
            out.next = StatusPage(Status::NotFound, parser);
            return Accepted::Accepted;
        }
        switch (parser.method) {
        case Method::Put:
            if (!parser.content_length.has_value()) {
                out.next = StatusPage(Status::LengthRequired, parser);
            } else {
                out.next = FilamentCommand(static_cast<uint8_t>(tool), *parser.content_length, parser.can_keep_alive(), parser.accepts_json);
            }
            return Accepted::Accepted;
        default:
            out.next = StatusPage(Status::MethodNotAllowed, parser);
            return Accepted::Accepted;
        }
    } else if (remove_prefix(suffix, "files").has_value()) {
        static const auto prefix = "/api/v1/files";
        static const size_t prefix_len = strlen(prefix);
        // We need both one SFN + one LFN in case of upload of a file.
        char filename[FILE_PATH_BUFFER_LEN + FILE_NAME_BUFFER_LEN + prefix_len];
        if (!parse_file_url(parser, prefix_len, filename, sizeof(filename), RemapPolicy::NoRemap, out)) {
            return Accepted::Accepted;
        }
        uint32_t etag = ChangedPath::instance.change_chain_hash(filename);
        if (etag == parser.if_none_match && etag != 0 /* 0 is special */ && (parser.method == Method::Get || parser.method == Method::Head)) {
            out.next = StatusPage(Status::NotModified, parser.status_page_handling(), parser.accepts_json, etag);
            return Accepted::Accepted;
        }
        switch (parser.method) {
        case Method::Put: {
            if (parser.create_folder) {
                out.next = create_folder(filename, parser);
                return Accepted::Accepted;
            } else {
                GcodeUpload::PutParams putParams;
                putParams.overwrite = parser.overwrite_file;
                putParams.print_after_upload = parser.print_after_upload;
                if (strlen(filename) + 1 > sizeof(putParams.filepath)) {
                    out.next = StatusPage(Status::UriTooLong, parser);
                    return Accepted::Accepted;
                }
                strlcpy(putParams.filepath.data(), filename, sizeof(putParams.filepath));
                auto upload = GcodeUpload::start(parser, wui_uploaded_gcode, parser.accepts_json, std::move(putParams));
                std::visit([&](auto upload) { out.next = std::move(upload); }, std::move(upload));
                return Accepted::Accepted;
            }
        }
        case Method::Get: {
            out.next = FileInfo(filename, parser.can_keep_alive(), parser.accepts_json, false, FileInfo::ReqMethod::Get, FileInfo::APIVersion::v1, etag);
            return Accepted::Accepted;
        }
        case Method::Head: {
            out.next = FileInfo(filename, parser.can_keep_alive(), parser.accepts_json, false, FileInfo::ReqMethod::Head, FileInfo::APIVersion::v1, etag);
            return Accepted::Accepted;
        }
        case Method::Delete: {
            out.next = delete_file(filename, parser);
            return Accepted::Accepted;
        }
        case Method::Post: {
            out.next = print_file(filename, parser);
            return Accepted::Accepted;
        }
        default:
            out.next = StatusPage(Status::MethodNotAllowed, StatusPage::CloseHandling::ErrorClose, parser.accepts_json);
            return Accepted::Accepted;
        }
    } else if (auto printer_suffix_opt = remove_prefix(suffix, "printer/"); printer_suffix_opt.has_value()) {
        // Argo: interactive controls — POST /api/v1/printer/{nozzle,bed,chamber-light}/<value>.
        if (parser.method != Method::Post) {
            out.next = StatusPage(Status::MethodNotAllowed, parser);
            return Accepted::Accepted;
        }
        const auto printer_suffix = *printer_suffix_opt;
        const auto slash = printer_suffix.find('/');
        if (slash == string_view::npos) {
            out.next = StatusPage(Status::BadRequest, parser);
            return Accepted::Accepted;
        }
        const auto key = printer_suffix.substr(0, slash);
        const auto value_str = printer_suffix.substr(slash + 1);
        int value = 0;
        if (from_chars_light(value_str.begin(), value_str.end(), value).ec != std::errc {}) {
            out.next = StatusPage(Status::BadRequest, parser);
            return Accepted::Accepted;
        }
        if (key == "nozzle") {
            set_nozzle_target(value, parser, out);
        } else if (key == "bed") {
            set_bed_target(value, parser, out);
#if HAS_SIDE_LEDS()
        } else if (key == "chamber-light") {
            set_chamber_light(value != 0, parser, out);
#endif
        } else {
            out.next = StatusPage(Status::NotFound, parser);
        }
        return Accepted::Accepted;
    } else {
        out.next = StatusPage(Status::NotFound, parser);
        return Accepted::Accepted;
    }
}

const PrusaLinkApiV1 prusa_link_api_v1;

} // namespace nhttp::link_content

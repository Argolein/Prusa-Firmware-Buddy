#include "screen_z_endstop_calibration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include <gui/frame_calibration_common.hpp>
#include <img_resources.hpp>

#include "marlin_server_extended_fsm_data.hpp"
#include <z_endstop_calibration_result.hpp>

static const char *text_header = N_("Z ENDSTOP CALIBRATION");

static constexpr auto center_frame_bottom = point_i16_t {
    rect_frame_bottom.Left() + rect_frame_bottom.Width() / 2,
    rect_frame_bottom.Top() + rect_frame_bottom.Height() / 2,
};

/// Common spinner + caption frame for the busy phases (homing / aligning / probing).
class FrameBusy {
private:
    window_text_t text;
    window_icon_hourglass_t spinner;

protected:
    FrameBusy(window_t *parent, const string_view_utf8 &caption)
        : text(parent, rect_frame_top, is_multiline::yes, is_closed_on_click_t::no, caption)
        , spinner(parent, center_frame_bottom) {
        spinner.SetRect(spinner.GetRect() - Rect16::Left_t(spinner.GetRect().Width() / 2));
        text.SetAlignment(Align_t::Center());
    }

public:
    void update(fsm::PhaseData) {}
};

class FrameHoming final : public FrameBusy {
public:
    explicit FrameHoming(window_t *parent)
        : FrameBusy(parent, _("Homing")) {}
};

class FrameAligning final : public FrameBusy {
public:
    explicit FrameAligning(window_t *parent)
        : FrameBusy(parent, _("Aligning Z motors")) {}
};

class FrameProbing final : public FrameBusy {
public:
    explicit FrameProbing(window_t *parent)
        : FrameBusy(parent, _("Probing near Z motors")) {}
};

class FrameResult final {
private:
    window_text_t text;
    std::array<char, 256> buffer;

    // Untranslated point labels: this is a developer calibration tool and the values are numeric.
    static constexpr const char *point_labels[] = { "Front left", "Rear center", "Front right" };

    static size_t append(std::array<char, 256> &buf, size_t n, const char *fmt, auto... args) {
        if (n >= buf.size()) {
            return buf.size() - 1;
        }
        const int w = snprintf(buf.data() + n, buf.size() - n, fmt, args...);
        return (w < 0) ? n : std::min(n + static_cast<size_t>(w), buf.size() - 1);
    }

public:
    explicit FrameResult(window_t *parent)
        : text(parent, rect_frame, is_multiline::yes, is_closed_on_click_t::no) {
        text.SetAlignment(Align_t::LeftTop());
    }

    void update(fsm::PhaseData) {
        ZEndstopCalibResult_t dt;
        if (!FSMExtendedDataManager::get(dt)) {
            return;
        }

        // Datum = lowest point. The other corners sit higher and are brought DOWN to meet it, so
        // every suggested adjustment is the same screw direction. With the user's endstop geometry
        // (CW = screw in = corner lower) that direction is CW; confirm once on the first run.
        size_t datum = 0;
        for (size_t i = 1; i < dt.z.size(); ++i) {
            if (dt.z[i] < dt.z[datum]) {
                datum = i;
            }
        }

        size_t n = 0;
        for (size_t i = 0; i < dt.z.size(); ++i) {
            if (i == datum) {
                n = append(buffer, n, "%-11s%+.3f  datum\n", point_labels[i], static_cast<double>(dt.z[i]));
            } else {
                const double dz = static_cast<double>(dt.z[i]) - static_cast<double>(dt.z[datum]);
                const double turns = dz / static_cast<double>(z_endstop_screw_pitch_mm);
                n = append(buffer, n, "%-11s%+.3f  %.2f turn CW\n", point_labels[i], static_cast<double>(dt.z[i]), turns);
            }
        }
        const bool ok = dt.spread <= z_endstop_calib_tolerance_mm;
        append(buffer, n, "Spread %.3f mm  %s", static_cast<double>(dt.spread), ok ? "OK" : "out of tolerance");

        text.SetText(string_view_utf8::MakeRAM(buffer.data()));
        text.Invalidate();
    }
};

class FrameProbeFailed final : public FrameInstructions {
public:
    explicit FrameProbeFailed(window_t *parent)
        : FrameInstructions(parent, _(text_probe_failed)) {}

    static constexpr const char *text_probe_failed = N_("Probing failed. Make sure the nozzle is clean and try again.");
};

using Frames = FrameDefinitionList<ScreenZEndstopCalibration::FrameStorage,
    FrameDefinition<PhasesZEndstopCalib::homing, FrameHoming>,
    FrameDefinition<PhasesZEndstopCalib::aligning, FrameAligning>,
    FrameDefinition<PhasesZEndstopCalib::probing, FrameProbing>,
    FrameDefinition<PhasesZEndstopCalib::show_result, FrameResult>,
    FrameDefinition<PhasesZEndstopCalib::probe_failed, FrameProbeFailed>>;

static PhasesZEndstopCalib get_phase(const fsm::BaseData &fsm_base_data) {
    return GetEnumFromPhaseIndex<PhasesZEndstopCalib>(fsm_base_data.GetPhase());
}

ScreenZEndstopCalibration::ScreenZEndstopCalibration()
    : ScreenFSM { text_header, rect_screen }
    , radio(this, rect_radio, PhasesZEndstopCalib::finish) {
    header.SetIcon(&img::calibrate_white_16x16);
    CaptureNormalWindow(radio);
    create_frame();
}

ScreenZEndstopCalibration::~ScreenZEndstopCalibration() {
    destroy_frame();
}

void ScreenZEndstopCalibration::create_frame() {
    Frames::create_frame(frame_storage, get_phase(fsm_base_data), this);
    radio.Change(get_phase(fsm_base_data));
}

void ScreenZEndstopCalibration::destroy_frame() {
    Frames::destroy_frame(frame_storage, get_phase(fsm_base_data));
}

void ScreenZEndstopCalibration::update_frame() {
    Frames::update_frame(frame_storage, get_phase(fsm_base_data), fsm_base_data.GetData());
}

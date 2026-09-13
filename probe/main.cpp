#include "SoftwareSurface.h"

#include <Application.h>
#include <Button.h>
#include <GroupLayout.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Size.h>
#include <String.h>
#include <TextControl.h>
#include <Window.h>

namespace {

enum : uint32 {
    kBack = 'back',
    kForward = 'frwd',
    kReload = 'reld',
    kNavigate = 'navg',
};

enum class NavigationIcon {
    kBack,
    kForward,
    kReload,
};

class NavigationButton final : public BButton {
public:
    NavigationButton(const char* name, const char* toolTip,
        NavigationIcon icon, BMessage* message)
        : BButton(name, "", message)
        , fIcon(icon)
    {
        SetToolTip(toolTip);
        SetExplicitMinSize(BSize(34, 28));
        SetExplicitMaxSize(BSize(34, B_SIZE_UNLIMITED));
    }

    void Draw(BRect updateRect) override
    {
        BButton::Draw(updateRect);

        const BRect bounds = Bounds();
        const float centerX = (bounds.left + bounds.right) / 2.0f;
        const float centerY = (bounds.top + bounds.bottom) / 2.0f;
        rgb_color color = ui_color(B_CONTROL_TEXT_COLOR);
        if (!IsEnabled())
            color = tint_color(color, B_DISABLED_LABEL_TINT);
        SetHighColor(color);
        SetPenSize(2.0f);
        SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);

        switch (fIcon) {
        case NavigationIcon::kBack:
            StrokeLine(BPoint(centerX + 6, centerY),
                BPoint(centerX - 5, centerY));
            StrokeLine(BPoint(centerX - 5, centerY),
                BPoint(centerX, centerY - 5));
            StrokeLine(BPoint(centerX - 5, centerY),
                BPoint(centerX, centerY + 5));
            break;
        case NavigationIcon::kForward:
            StrokeLine(BPoint(centerX - 6, centerY),
                BPoint(centerX + 5, centerY));
            StrokeLine(BPoint(centerX + 5, centerY),
                BPoint(centerX, centerY - 5));
            StrokeLine(BPoint(centerX + 5, centerY),
                BPoint(centerX, centerY + 5));
            break;
        case NavigationIcon::kReload: {
            const BRect arc(centerX - 6, centerY - 6,
                centerX + 6, centerY + 6);
            StrokeArc(arc, 35.0f, 285.0f);
            StrokeLine(BPoint(centerX + 6, centerY - 3),
                BPoint(centerX + 6, centerY - 8));
            StrokeLine(BPoint(centerX + 6, centerY - 3),
                BPoint(centerX + 1, centerY - 3));
            break;
        }
        }
        SetPenSize(1.0f);
    }

private:
    NavigationIcon fIcon;
};

class BrowserWindow final : public BWindow {
public:
    BrowserWindow()
        : BWindow(BRect(80, 80, 980, 700), "R Chromium native surface probe",
              B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE)
    {
        fAddress = new BTextControl("address", nullptr, "https://example.com",
            new BMessage(kNavigate));
        fAddress->SetModificationMessage(nullptr);

        auto* surface = new SoftwareSurface("software surface");
        BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
            .AddGroup(B_HORIZONTAL, 4)
                .SetInsets(B_USE_DEFAULT_SPACING)
                .Add(new NavigationButton("back", "Back",
                    NavigationIcon::kBack, new BMessage(kBack)))
                .Add(new NavigationButton("forward", "Forward",
                    NavigationIcon::kForward, new BMessage(kForward)))
                .Add(new NavigationButton("reload", "Reload",
                    NavigationIcon::kReload, new BMessage(kReload)))
                .Add(fAddress, 1)
            .End()
            .Add(surface, 1);
    }

    void MessageReceived(BMessage* message) override
    {
        switch (message->what) {
        case kBack:
            SetTitle("R Chromium native probe — Back input received");
            break;
        case kForward:
            SetTitle("R Chromium native probe — Forward input received");
            break;
        case kReload:
            SetTitle("R Chromium native probe — Reload input received");
            break;
        case kNavigate: {
            BString title("R Chromium native probe — ");
            title << fAddress->Text();
            SetTitle(title.String());
            break;
        }
        default:
            BWindow::MessageReceived(message);
            break;
        }
    }

private:
    BTextControl* fAddress {nullptr};
};

class NativeProbeApplication final : public BApplication {
public:
    NativeProbeApplication()
        : BApplication("application/x-vnd.rchromium-native-probe")
    {
    }

    void ReadyToRun() override
    {
        (new BrowserWindow())->Show();
    }
};

}  // namespace

int main()
{
    NativeProbeApplication app;
    app.Run();
    return 0;
}

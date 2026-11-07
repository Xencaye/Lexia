#include "native_ui/pages/report_page.h"

#include "native_ui/pages/layout.h"
#include "strings/string_ids.h"

namespace nui {

ReportPage::ReportPage(const PageEnv& env) : PageView(Page::Report, env) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    SetName(T(strings::id::kWindowNavReport));

    PictureWidget* dog = Add<PictureWidget>("dog", PictureTint::Dog);
    dog->SetOpacity(layout::kDogOpacity);
    dog->SetBounds(layout::kDog);

    // Two lines, one per line of the string (`white-space: pre-line`): a Paragraph with no gap.
    Paragraph* title = Add<Paragraph>();
    title->SetText(T(strings::id::kWindowReportTitle));
    title->SetStyle(layout::kBigFont, FontWeight::Bold);
    title->SetLineHeight(layout::kBigLine);
    title->SetParagraphGap(0);
    title->SetLetterSpacing(layout::kBigLetterSpacing);
    title->SetBounds({layout::kBigX, layout::kBigY, layout::kWindowW - layout::kBigX, 2 * layout::kBigLine});

    Paragraph* text = Add<Paragraph>();
    text->SetText(T(strings::id::kWindowReportText));
    text->SetLineHeight(layout::kBodyLine);
    text->SetBounds({layout::kReportTextX, layout::kReportTextY, layout::kReportTextW, layout::kBodyLine});

    mail_ = AddLink(strings::id::kWindowReportMail, "mail-01", LinkId::Mail);
    discord_ = AddLink(strings::id::kWindowReportDiscord, "discord", LinkId::Discord);
    github_ = AddLink(strings::id::kWindowReportGithub, "github", LinkId::Github);
}

TextLink* ReportPage::AddLink(const char* textId, const char* icon, LinkId id) {
    TextLink* l = Add<TextLink>(T(textId), icon);
    l->SetFontSize(layout::kReportLinkFont);
    l->SetIconSize(layout::kReportLinkIcon);
    l->SetGap(layout::kReportLinkIconGap);
    l->SetReserveBold(false);  // a plain `.text-link` is never bold, so its width is the regular word's
    l->SetOnActivate([this, id] {
        PageMessage m = MakeMessage(PageMessageType::OpenLink, "openLink");
        m.link = id;  // the page never sends a URL: the host maps the id to its fixed address
        Emit(m);
    });
    return l;
}

// The three links in one row, 29 apart, each centred on the row (26 tall).
void ReportPage::Relayout(Canvas& canvas) {
    float x = layout::kReportLinksX;
    for (TextLink* l : {mail_, discord_, github_}) {
        const SizeF s = l->Measure(canvas, 10000);
        l->SetBounds({x, layout::kReportLinksY + (layout::kReportLinksH - s.h) / 2, s.w, s.h});
        x += s.w + layout::kReportLinksGap;
    }
}

}  // namespace nui

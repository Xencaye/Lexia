#include "native_ui/pages/about_page.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "native_ui/pages/layout.h"
#include "native_ui/text_layout.h"
#include "strings/string_ids.h"

namespace nui {

namespace {

// The design keeps "video games" and "comics" on one line (`strong { white-space: nowrap }`). The wrapper
// breaks at spaces only, so the space inside a bold phrase becomes a no-break space (U+00A0).
std::string NoBreak(std::string s) {
    for (size_t pos = s.find(' '); pos != std::string::npos; pos = s.find(' ', pos)) {
        s.replace(pos, 1, "\xC2\xA0");
        pos += 2;
    }
    return s;
}

}  // namespace

AboutPage::AboutPage(const PageEnv& env) : PageView(Page::About, env) {
    SetBounds({0, 0, layout::kWindowW, layout::kWindowH});
    SetName(T(strings::id::kWindowNavAbout));

    PictureWidget* wolf = Add<PictureWidget>("wolf", PictureTint::Dog);
    wolf->SetOpacity(layout::kWolfOpacity);
    wolf->SetBounds(layout::kWolf);

    // The head: the name in bold accent, then " | ver. x" in ink (placed in Relayout, from their widths).
    name_ = Add<Label>(T(strings::id::kWindowAboutName));
    name_->SetStyle(layout::kAboutHeadFont, FontWeight::Bold);
    name_->SetColour(ColourRole::Accent);
    version_ = Add<Label>();
    version_->SetStyle(layout::kAboutHeadFont);

    // The intro and the hope: one Paragraph, two paragraphs 11 apart, 300 wide. Two words of the intro are bold.
    Paragraph* top = Add<Paragraph>();
    top->SetText(T(strings::id::kWindowAboutIntro) + "\n" + T(strings::id::kWindowAboutHope),
                 {{"games", NoBreak(T(strings::id::kWindowAboutGames))},
                  {"comics", NoBreak(T(strings::id::kWindowAboutComics))}});
    top->SetLineHeight(layout::kBodyLine);
    top->SetParagraphGap(layout::kAboutTopGap);
    // Room for four lines and the gap; the text itself is as long as it is.
    top->SetBounds({layout::kAboutTopX, layout::kAboutTopY, layout::kAboutTopW,
                    4 * layout::kBodyLine + layout::kAboutTopGap});

    // The text that goes round the wolf: three paragraphs, the first lines indented by the picture's edge.
    Paragraph* more = Add<Paragraph>();
    more->SetText(T(strings::id::kWindowAboutLocal) + "\n" + T(strings::id::kWindowAboutHuman) + "\n" +
                  T(strings::id::kWindowAboutThanks));
    more->SetStyle(layout::kAboutMoreFont);
    more->SetLineHeight(layout::kAboutMoreLine);
    more->SetParagraphGap(layout::kAboutMoreGap);
    more->SetIndents(std::vector<float>(std::begin(layout::kAboutIndents), std::end(layout::kAboutIndents)));
    more->SetBounds({layout::kAboutMoreX, layout::kAboutMoreY, layout::kAboutMoreW,
                     9 * layout::kAboutMoreLine + 2 * layout::kAboutMoreGap});

    itch_ = AddIconLink("itch", strings::id::kWindowAboutItch, LinkId::Itch);
    github_ = AddIconLink("github", strings::id::kWindowAboutGithub, LinkId::Github);
    itch_->SetBounds({layout::kAboutLinksX, layout::kAboutLinksY, 28, 28});
    github_->SetBounds({layout::kAboutLinksX + 28 + layout::kAboutLinksGap, layout::kAboutLinksY, 28, 28});

    // The two small lines. The second is "View third-party notices {link}.": the link word sits where the
    // placeholder is, so the template is cut there and the three parts are placed in a row.
    legal_ = Add<Label>(T(strings::id::kWindowAboutLicence));
    legal_->SetStyle(layout::kAboutLegalFont);
    const std::string line = T(strings::id::kWindowAboutNoticesLine);
    const size_t at = line.find("{link}");
    noticesTemplateBefore_ = at == std::string::npos ? line : line.substr(0, at);
    noticesTemplateAfter_ = at == std::string::npos ? std::string() : line.substr(at + 6);
    noticesBefore_ = Add<Label>(noticesTemplateBefore_);
    noticesBefore_->SetStyle(layout::kAboutLegalFont);
    noticesBefore_->SetAccessHidden(true);  // the link below is called "View the third-party notices" in full
    notices_ = Add<TextLink>(T(strings::id::kWindowAboutNotices));
    notices_->SetSmall(true);  // always underlined
    notices_->SetFontSize(layout::kAboutLegalFont);
    notices_->SetReserveBold(false);
    notices_->SetName(T(strings::id::kWindowAboutNoticesLabel));
    notices_->SetOnActivate([this] { Emit(MakeMessage(PageMessageType::OpenNotices, "openNotices")); });
    noticesAfter_ = Add<Label>(noticesTemplateAfter_);
    noticesAfter_->SetStyle(layout::kAboutLegalFont);
    noticesAfter_->SetAccessHidden(true);  // only the full stop after the link
}

IconButton* AboutPage::AddIconLink(const char* icon, const char* nameId, LinkId id) {
    IconButton* b = Add<IconButton>(icon);
    b->SetIdleAccent(true);
    b->SetName(T(nameId));
    b->SetTooltip(T(nameId));
    b->SetOnActivate([this, id] {
        PageMessage m = MakeMessage(PageMessageType::OpenLink, "openLink");
        m.link = id;  // the page never sends a URL: the host maps the id to its fixed address
        Emit(m);
    });
    return b;
}

void AboutPage::SetState(const UiState& state) {
    const std::string text = "| " + F(strings::id::kWindowAboutVersion, {{"version", state.version}});
    if (text != versionText_) {
        versionText_ = text;
        version_->SetText(text);
        MarkLayout();
    }
}

void AboutPage::Relayout(Canvas& canvas) {
    // The head on one line box 30 tall: the name, a space, the rest.
    TextStyle regular;
    regular.size = layout::kAboutHeadFont;
    const SizeF nameSize = name_->Measure(canvas, 10000);
    name_->SetBounds({layout::kAboutHeadX, layout::kAboutHeadY, nameSize.w + 2, layout::kAboutHeadLine});
    const float x = layout::kAboutHeadX + nameSize.w + SpaceWidth(canvas, regular);
    version_->SetBounds({x, layout::kAboutHeadY, std::max(0.0f, layout::kWindowW - x), layout::kAboutHeadLine});

    // The legal lines: 13 high each, the second built from three parts.
    TextStyle legalStyle;
    legalStyle.size = layout::kAboutLegalFont;
    legal_->SetBounds({layout::kAboutLegalX, layout::kAboutLegalY, 300, layout::kAboutLegalLine});
    const float y2 = layout::kAboutLegalY + layout::kAboutLegalLine;
    // A text engine may trim a trailing space off a measured string, so it is taken off and added back.
    std::string before = noticesTemplateBefore_;
    bool spaceBeforeLink = false;
    while (!before.empty() && before.back() == ' ') {
        before.pop_back();
        spaceBeforeLink = true;
    }
    float x2 = layout::kAboutLegalX;
    const float bw = canvas.MeasureText(before, legalStyle).w;
    noticesBefore_->SetText(before);
    noticesBefore_->SetBounds({x2, y2, bw + 2, layout::kAboutLegalLine});
    x2 += bw + (spaceBeforeLink ? SpaceWidth(canvas, legalStyle) : 0.0f);
    const SizeF ls = notices_->Measure(canvas, 10000);
    notices_->SetBounds({x2, y2 + (layout::kAboutLegalLine - ls.h) / 2, ls.w, ls.h});
    x2 += ls.w;
    const float aw = canvas.MeasureText(noticesTemplateAfter_, legalStyle).w;
    noticesAfter_->SetBounds({x2, y2, aw + 2, layout::kAboutLegalLine});
}

}  // namespace nui

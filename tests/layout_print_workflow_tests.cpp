#include "acp/document.hpp"
#include "acp/layout.hpp"
#include "acp/pdf.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {
int failures = 0;

void expect(bool condition, const char* name) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}
}

int main() {
    using namespace acp;

    Document document;
    BlockLibrary blocks;

    (void)document.insert(
        PolylineEntity{{
            {0.0, 0.0},
            {10000.0, 0.0},
            {10000.0, 5000.0},
            {0.0, 5000.0}}, true});

    layout::PageSetup a3Landscape{
        layout::PaperSize::A3,
        layout::Orientation::Landscape,
        {10.0, 10.0, 10.0, 10.0}
    };

    const auto paper =
        layout::paper_size_mm(a3Landscape.paper, a3Landscape.orientation);
    expect(paper.width == 420.0 && paper.height == 297.0,
           "A3 landscape paper size");

    const auto fit =
        layout::fit_to_page(
            *bounds::drawing_bounds(document),
            a3Landscape);
    expect(fit.has_value(), "fit viewport exists");

    const auto pdfFit =
        pdf::export_document(
            document,
            &blocks,
            a3Landscape,
            std::nullopt);
    expect(pdfFit.has_value(), "fit PDF export succeeds");
    if (pdfFit.has_value()) {
        expect(pdfFit->find("/MediaBox [0 0") != std::string::npos,
               "PDF contains media box");
    }

    const auto pdfScale50 =
        pdf::export_document(
            document,
            &blocks,
            a3Landscape,
            50.0);
    expect(pdfScale50.has_value(), "1:50 PDF fits A3 landscape");

    const auto pdfScale100 =
        pdf::export_document(
            document,
            &blocks,
            a3Landscape,
            100.0);
    expect(pdfScale100.has_value(), "1:100 PDF fits A3 landscape");

    layout::PageSetup a4Portrait{
        layout::PaperSize::A4,
        layout::Orientation::Portrait,
        {10.0, 10.0, 10.0, 10.0}
    };

    const auto tooLargeAt50 =
        pdf::export_document(
            document,
            &blocks,
            a4Portrait,
            50.0);
    expect(!tooLargeAt50.has_value(),
           "reject fixed scale when drawing exceeds printable area");

    const auto fitsAt100 =
        pdf::export_document(
            document,
            &blocks,
            a4Portrait,
            100.0);
    expect(fitsAt100.has_value(),
           "accept fixed scale when drawing fits printable area");

    expect(!pdf::export_document(
                document,
                &blocks,
                a3Landscape,
                0.0).has_value(),
           "reject zero print scale");

    expect(!pdf::export_document(
                document,
                &blocks,
                a3Landscape,
                -100.0).has_value(),
           "reject negative print scale");

    if (failures != 0) {
        std::cerr << failures << " layout/print regression test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All layout/print regression tests passed\n";
    return EXIT_SUCCESS;
}

#!/usr/bin/env python3
"""Create website demo assets. Tooling only: pip install reportlab pymupdf."""
from pathlib import Path

import pymupdf
from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs" / "assets"
INK = HexColor("#172554")
MUTED = HexColor("#475569")
BLUE = HexColor("#2457d6")
TEAL = HexColor("#087f75")
LINE = HexColor("#ccd8e6")
WHITE = HexColor("#ffffff")


def create_pdf(path):
    pdf = canvas.Canvas(str(path), pagesize=A4, invariant=True)
    width, height = A4
    pdf.setTitle("Project approval - jPDF Desk demo")
    pdf.setAuthor("jPDF Desk")
    form = pdf.acroForm

    def text(x, y, value, size=11, color=INK, font="Helvetica"):
        pdf.setFillColor(color)
        pdf.setFont(font, size)
        pdf.drawString(x, y, value)

    def section(y, number, title):
        text(44, y, number, 10, TEAL, "Helvetica-Bold")
        text(70, y, title, 14, INK, "Helvetica-Bold")

    def field(x, y, label, name, field_width):
        text(x, y + 39, label, 10, MUTED)
        form.textfield(name=name, tooltip=label, x=x, y=y,
                       width=field_width, height=30, fontSize=12,
                       textColor=INK, borderColor=LINE, fillColor=WHITE,
                       borderWidth=1, forceBorder=True)

    pdf.setFillColor(INK)
    pdf.rect(0, height - 169, width, 169, fill=1, stroke=0)
    text(44, height - 44, "JPDF DESK / SAMPLE DOCUMENT", 10,
         HexColor("#9ee9df"), "Helvetica-Bold")
    text(44, height - 88, "Make room for", 30, WHITE, "Helvetica-Bold")
    text(44, height - 124, "good ideas.", 30, WHITE, "Helvetica-Bold")
    text(44, 646, "PROJECT APPROVAL", 10, TEAL, "Helvetica-Bold")
    text(44, 624, "A simple plan. A clear next step.", 12, MUTED)

    section(585, "01", "Project details")
    field(44, 522, "Full name", "full_name", 242)
    field(309, 522, "Email address", "email", 242)
    field(44, 456, "Project name", "project", 507)
    text(44, 429, "Project stage", 10, MUTED)
    form.choice(name="stage", tooltip="Project stage", x=44, y=390,
                width=242, height=30, options=["Planning", "In progress", "Ready for review"],
                value="Planning", fontSize=12, textColor=INK, borderColor=LINE,
                fillColor=WHITE, borderWidth=1, forceBorder=True)
    field(309, 390, "Target date", "target_date", 242)

    section(355, "02", "Review checklist")
    form.checkbox(name="reviewed", tooltip="Project details reviewed", x=44, y=318,
                  size=15, checked=False, buttonStyle="check", borderColor=LINE,
                  fillColor=WHITE, textColor=BLUE, forceBorder=True)
    text(70, 321, "Project details reviewed", 11)
    pdf.setStrokeColor(LINE)
    pdf.rect(44, 289, 15, 15, fill=0, stroke=1)
    text(70, 292, "Ready for the next step", 11)

    section(255, "03", "Notes & approval")
    pdf.setFillColor(HexColor("#f3f6fb"))
    pdf.roundRect(44, 149, 507, 83, 5, fill=1, stroke=0)
    text(58, 211, "Notes", 10, MUTED)
    pdf.setStrokeColor(LINE)
    pdf.line(44, 81, 350, 81)
    pdf.line(388, 81, 551, 81)
    text(44, 65, "Signature", 10, MUTED)
    text(388, 65, "Date", 10, MUTED)
    text(44, 28, "Demo only / Use fictional details and a sample signature", 8, MUTED)
    pdf.showPage()
    pdf.save()


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / "project-approval-demo.pdf"
    create_pdf(path)
    with pymupdf.open(path) as doc:
        doc[0].get_pixmap(matrix=pymupdf.Matrix(1.5, 1.5)).save(
            str(OUTPUT / "project-approval-preview.png"))
    print(f"Created demo PDF and preview in {OUTPUT}")


if __name__ == "__main__":
    main()

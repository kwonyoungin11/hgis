#include "ExportService.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStringConverter>

QString ExportService::exportSubmissionPackage(const QString& outDir,
                                               const QString& encoding,
                                               const QString& checklistSummary,
                                               bool blockOnError,
                                               bool hasChecklistErrors,
                                               QString* errorOut) {
  if (blockOnError && hasChecklistErrors) {
    if (errorOut) *errorOut = QStringLiteral("Checklist errors remain; export blocked.");
    return {};
  }
  QDir dir(outDir);
  if (!dir.exists() && !QDir().mkpath(outDir)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot create output folder");
    return {};
  }
  const QString readme = dir.filePath(QStringLiteral("README_submit.txt"));
  QFile f(readme);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot write README");
    return {};
  }
  QTextStream ts(&f);
  ts.setEncoding(QStringConverter::Utf8);
  ts << "KA-HGIS submission package\n";
  ts << "created: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
  ts << "crs: EPSG:5179\n";
  ts << "shp_encoding: " << encoding << "\n\n";
  ts << "checklist:\n" << checklistSummary << "\n";
  f.close();

  QFile enc(dir.filePath(QStringLiteral("encoding.txt")));
  if (enc.open(QIODevice::WriteOnly | QIODevice::Text)) {
    enc.write(encoding.toUtf8());
    enc.write("\n");
  }
  return outDir;
}

QString ExportService::writePdfPlaceholder(const QString& outPath, const QString& title, QString* errorOut) {
  QFile f(outPath);
  if (!f.open(QIODevice::WriteOnly)) {
    if (errorOut) *errorOut = QStringLiteral("Cannot write PDF");
    return {};
  }
  const QByteArray content = title.toUtf8();
  QByteArray pdf;
  pdf += "%PDF-1.1\n";
  pdf += "1 0 obj<< /Type /Catalog /Pages 2 0 R >>endobj\n";
  pdf += "2 0 obj<< /Type /Pages /Kids [3 0 R] /Count 1 >>endobj\n";
  pdf += "3 0 obj<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources<< /Font<< /F1 5 0 R >> >> >>endobj\n";
  QByteArray stream = "BT /F1 24 Tf 100 700 Td (";
  for (char c : content) {
    if (c == '(' || c == ')' || c == '\\') stream += '\\';
    if (static_cast<unsigned char>(c) < 128) stream += c; else stream += '?';
  }
  stream += ") Tj ET\n";
  pdf += "4 0 obj<< /Length " + QByteArray::number(stream.size()) + " >>stream\n";
  pdf += stream;
  pdf += "endstream endobj\n";
  pdf += "5 0 obj<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>endobj\n";
  pdf += "xref\n0 6\n0000000000 65535 f \n";
  pdf += "trailer<< /Size 6 /Root 1 0 R >>\nstartxref\n0\n%%EOF\n";
  f.write(pdf);
  f.close();
  return outPath;
}

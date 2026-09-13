#include "HeritageFormParser.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>

namespace {

QString attr(const QString& tag, const QString& name) {
  // name="..."  또는  name='...'  또는  name=값
  const QRegularExpression re(
      QStringLiteral("\\b%1\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))").arg(name),
      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch m = re.match(tag);
  if (!m.hasMatch()) return {};
  for (int i = 1; i <= 3; ++i) {
    if (!m.captured(i).isNull()) return m.captured(i);
  }
  return {};
}

QString stripTags(const QString& text) {
  QString out = text;
  out.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
  out.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
  out.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  return out.trimmed();
}

// 공백을 모두 지운다. 화면 글자는 줄바꿈·들여쓰기가 섞여 온다.
QString squash(const QString& text) {
  QString out = text;
  out.remove(QRegularExpression(QStringLiteral("\\s+")));
  return out;
}

// 시/도 표기의 꼬리를 떼고 별칭을 맞춘다.
// 「강원도」 ↔ 「강원특별자치도」, 「전라북도」 ↔ 「전북특별자치도」 처럼 사이트와 앱 표기가 다르다.
QString sidoCore(const QString& raw) {
  QString s = squash(raw);
  for (const QString& tail : {QStringLiteral("특별자치도"), QStringLiteral("특별자치시"),
                              QStringLiteral("특별시"), QStringLiteral("광역시")}) {
    if (s.endsWith(tail)) {
      s.chop(tail.size());
      break;
    }
  }
  if (s.size() > 2 && s.endsWith(QStringLiteral("도"))) s.chop(1);
  static const QMap<QString, QString> alias{
      {QStringLiteral("전라북"), QStringLiteral("전북")},
      {QStringLiteral("전라남"), QStringLiteral("전남")},
      {QStringLiteral("경상북"), QStringLiteral("경북")},
      {QStringLiteral("경상남"), QStringLiteral("경남")},
      {QStringLiteral("충청북"), QStringLiteral("충북")},
      {QStringLiteral("충청남"), QStringLiteral("충남")},
  };
  return alias.value(s, s);
}

struct SelectBlock {
  QString name;
  QString id;
  QMap<QString, QString> options;  // 보이는 글자 → 값
  int at = -1;
};

QList<SelectBlock> selectsIn(const QString& html) {
  QList<SelectBlock> out;
  const QRegularExpression re(QStringLiteral("<select\\b([^>]*)>(.*?)</select>"),
                              QRegularExpression::CaseInsensitiveOption |
                                  QRegularExpression::DotMatchesEverythingOption);
  QRegularExpressionMatchIterator it = re.globalMatch(html);
  while (it.hasNext()) {
    const QRegularExpressionMatch m = it.next();
    SelectBlock block;
    block.name = attr(m.captured(1), QStringLiteral("name"));
    block.id = attr(m.captured(1), QStringLiteral("id"));
    block.at = m.capturedStart();

    const QRegularExpression opt(QStringLiteral("<option\\b([^>]*)>(.*?)</option>"),
                                 QRegularExpression::CaseInsensitiveOption |
                                     QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator oi = opt.globalMatch(m.captured(2));
    while (oi.hasNext()) {
      const QRegularExpressionMatch om = oi.next();
      const QString label = stripTags(om.captured(2));
      QString value = attr(om.captured(1), QStringLiteral("value"));
      if (value.isNull()) value = label;
      if (!label.isEmpty()) block.options.insert(label, value);
    }
    out.append(block);
  }
  return out;
}

}  // namespace

HeritageForm HeritageFormParser::parse(const QByteArray& html, const QString& anchor) {
  HeritageForm out;
  const QString text = QString::fromUtf8(html);
  const QString want = anchor.toLower();

  // anchor 를 품은 form 블록을 고른다. 지도 패널(bjdcd*)의 폼을 잘못 잡지 않기 위해서다.
  const QRegularExpression formRe(QStringLiteral("<form\\b([^>]*)>(.*?)</form>"),
                                  QRegularExpression::CaseInsensitiveOption |
                                      QRegularExpression::DotMatchesEverythingOption);
  QString head;
  QString body;
  QRegularExpressionMatchIterator fi = formRe.globalMatch(text);
  while (fi.hasNext()) {
    const QRegularExpressionMatch fm = fi.next();
    if (!fm.captured(2).toLower().contains(want)) continue;
    head = fm.captured(1);
    body = fm.captured(2);
    break;
  }
  if (body.isEmpty()) {
    out.warnings << QStringLiteral("'%1' 칸을 품은 폼을 찾지 못했습니다.").arg(anchor);
    return out;
  }

  out.action = attr(head, QStringLiteral("action"));
  const QString method = attr(head, QStringLiteral("method")).toUpper();
  if (!method.isEmpty()) out.method = method;

  // hidden 을 포함한 모든 input 을 그대로 되돌려 보낸다. 서버가 기대하는 값이 있다.
  const QRegularExpression inputRe(QStringLiteral("<input\\b([^>]*)>"),
                                   QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatchIterator ii = inputRe.globalMatch(body);
  while (ii.hasNext()) {
    const QString tag = ii.next().captured(1);
    const QString name = attr(tag, QStringLiteral("name"));
    if (name.isEmpty()) continue;
    const QString type = attr(tag, QStringLiteral("type")).toLower();
    if (type == QLatin1String("submit") || type == QLatin1String("button")) continue;
    // 체크되지 않은 것은 보내지 않는다. 브라우저와 같은 규칙이다.
    if ((type == QLatin1String("checkbox") || type == QLatin1String("radio")) &&
        !tag.contains(QStringLiteral("checked"), Qt::CaseInsensitive))
      continue;
    out.fields.insert(name, attr(tag, QStringLiteral("value")));
  }

  const QList<SelectBlock> sels = selectsIn(body);
  int anchorAt = -1;
  for (const SelectBlock& s : sels) {
    const QString key = (s.id + QLatin1Char(' ') + s.name).toLower();
    if (key.contains(want)) {
      out.sidoField = s.name;
      out.sidoOptions = s.options;
      anchorAt = s.at;
      break;
    }
  }
  if (out.sidoField.isEmpty()) {
    out.warnings << QStringLiteral("'%1' select 를 찾지 못했습니다.").arg(anchor);
    return out;
  }

  // 시/군/구는 같은 폼에서 시/도 **바로 다음**에 오는 select 다.
  for (const SelectBlock& s : sels) {
    if (s.at <= anchorAt || s.name.isEmpty()) continue;
    out.sigunguField = s.name;
    out.sigunguOptions = s.options;
    break;
  }
  if (out.sigunguField.isEmpty())
    out.warnings << QStringLiteral("시·군·구 select 를 찾지 못했습니다.");

  // select 의 현재 값도 함께 보낸다(선택된 것이 없으면 빈 값).
  for (const SelectBlock& s : sels) {
    if (!s.name.isEmpty() && !out.fields.contains(s.name)) out.fields.insert(s.name, QString());
  }
  return out;
}

QString HeritageFormParser::matchOption(const QMap<QString, QString>& options,
                                        const QString& wanted) {
  if (options.isEmpty() || wanted.trimmed().isEmpty()) return {};
  const QString w = squash(wanted);

  for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
    if (squash(it.key()) == w) return it.value();
  }
  for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
    const QString label = squash(it.key());
    if (label.contains(w) || w.contains(label)) return it.value();
  }
  // 시/도 표기 차이(강원도 ↔ 강원특별자치도 등).
  const QString core = sidoCore(wanted);
  for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
    if (sidoCore(it.key()) == core) return it.value();
  }
  // 전라남도·광주광역시는 사이트에서 「전남광주통합특별시」로 합쳐져 있다.
  if (core == QStringLiteral("전남") || core == QStringLiteral("광주")) {
    for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
      if (squash(it.key()).contains(QStringLiteral("전남광주"))) return it.value();
    }
  }
  return {};
}

int HeritageFormParser::resultCount(const QByteArray& html) {
  const QString text = stripTags(QString::fromUtf8(html));
  const QRegularExpression re(QStringLiteral("검색결과[^0-9]*([0-9,]+)"));
  const QRegularExpressionMatch m = re.match(text);
  if (!m.hasMatch()) return -1;
  QString digits = m.captured(1);
  digits.remove(QLatin1Char(','));
  bool ok = false;
  const int n = digits.toInt(&ok);
  return ok ? n : -1;
}

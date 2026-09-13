#pragma once

#include <QFrame>
#include <QListWidget>
#include <QString>
#include <QStringList>

class QLabel;
class QListWidgetItem;

// 파일함 목록. 항목을 지도로 끌어 놓으면 레이어가 되도록 드래그를 켜 둔다.
class KaFileListView : public QListWidget {
  Q_OBJECT
public:
  explicit KaFileListView(QWidget* parent = nullptr);

protected:
  void startDrag(Qt::DropActions supportedActions) override;
};

// 「파일함」 카드 통째. 지도 창과 조판 창이 같은 것을 쓴다.
// 경로 이동만 스스로 하고, 파일을 열어 레이어로 만드는 일은 바깥(MainWindow)이 한다.
class KaFileBrowserPanel : public QFrame {
  Q_OBJECT
public:
  explicit KaFileBrowserPanel(QWidget* parent = nullptr);

  KaFileListView* listView() const { return m_list; }
  QString currentPath() const { return m_path; }
  // 고른 것 중 폴더가 아닌 파일 경로만 준다(지도로 끌어 놓기용).
  QStringList selectedFiles() const;

public slots:
  // 빈 경로면 드라이브 목록으로 간다.
  void goTo(const QString& path);
  void goUp();
  void pickFolder();

signals:
  // 파일을 두 번 눌렀다 — 바깥에서 레이어로 연다.
  void fileActivated(const QString& path);
  // 상태막대에 그대로 띄울 한 줄.
  void statusMessage(const QString& text);

public:
  // 바탕화면은 OneDrive로 옮겨 가 있는 현장 PC가 많아 후보를 차례로 찾는다.
  static QString resolvedDesktopPath();
  static QString findRemovableSdPath();

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;

private:
  void updateButtonLayout(int width);
  void fitShortcutFonts(int width);

  KaFileListView* m_list = nullptr;
  QString m_path;
  class QToolButton* m_btnPc = nullptr;
  class QToolButton* m_btnDesk = nullptr;
  class QToolButton* m_btnDocs = nullptr;
  class QToolButton* m_btnDown = nullptr;
  class QToolButton* m_btnPick = nullptr;
  class QToolButton* m_btnSd = nullptr;
  class QToolButton* m_btnUp = nullptr;
  class QHBoxLayout* m_pathBar1 = nullptr;
  class QHBoxLayout* m_pathBar2 = nullptr;
  bool m_isTwoRows = false;
};

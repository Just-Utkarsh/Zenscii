#pragma once

#include <QImage>
#include <QMainWindow>
#include <QVector>

class QLabel;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QScrollArea;
class QTimer;

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);

protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
  void onOpenImage();
  void onSavePreview();
  void onSaveText();
  void onOpenVideo();
  void onToggleVideoPreview();
  void onSaveAsciiVideoMp4();
  void onVideoTick();
  void onSettingsChanged();
  void onZoomChanged();

private:
  enum class ColorMode { None, Ansi256, Ansi24 };

  void setStatus(const QString& s);
  void recomputePreview();
  void updatePreviewPixmap();
  QString renderAsciiText(const QImage& src) const;
  QImage loadImageAny(const QString& path, QString* err) const;
  void showDisplayImage(const QImage& img);

  // Conversion/render
  QImage renderAsciiImage(const QImage& src) const;
  QChar rampChar(double t01) const;

  // UI
  QLabel* preview_ = nullptr;
  QScrollArea* scroll_ = nullptr;
  QLineEdit* path_ = nullptr;
  QSpinBox* width_ = nullptr;
  QDoubleSpinBox* aspect_ = nullptr;
  QDoubleSpinBox* zoom_ = nullptr;
  QLineEdit* chars_ = nullptr;
  QComboBox* color_ = nullptr;
  QCheckBox* bg_ = nullptr;
  QCheckBox* invert_ = nullptr;
  QPushButton* openBtn_ = nullptr;
  QPushButton* saveBtn_ = nullptr;
  QPushButton* saveTextBtn_ = nullptr;
  QPushButton* openVideoBtn_ = nullptr;
  QPushButton* previewVideoBtn_ = nullptr;
  QPushButton* saveVideoBtn_ = nullptr;
  QDoubleSpinBox* videoFps_ = nullptr;
  QSpinBox* videoMaxFrames_ = nullptr;

  // State
  QImage src_;
  QImage previewImg_;
  QImage displayImg_;

  // Video state (rendered ASCII frames)
  QString videoPath_;
  QVector<QImage> videoFrames_;
  int videoFrameIdx_ = 0;
  bool videoPlaying_ = false;
  QTimer* videoTimer_ = nullptr;
  double videoFpsActual_ = 10.0;
};


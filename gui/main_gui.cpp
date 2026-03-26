#include "MainWindow.h"

#include <QApplication>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  MainWindow w;
  w.resize(1100, 800);
  w.show();
  return app.exec();
}


#pragma once

#include <QMainWindow>
#include <QMdiSubWindow>
#include <QTimer>
#include "qcustomplot.h"

#include "ControlBinder.h"
#include "Oscilloscope.h"
#include "ImageRenderWindow.h"
#include "LifetimeDisplayWidget.h"
#include "SimTcspc.h"
#include "FlimFileWriter.h"
#include "FlimWorkspace.h"
#include "FlimServer.h"

#include "ui_FlimDisplay.h"
#include <memory>




class FlimDisplay : public QMainWindow, private ControlBinder, private Ui::FlimDisplay
{
   Q_OBJECT

public:
   explicit FlimDisplay();
   ~FlimDisplay();

   void shutdown();
   void setStatusBarMessage(const QString& message) { statusbar->showMessage(message); }
   void setLive(bool live);
   void updateProgress(double progress);
   void showTcspcSettings();

signals:
   void statusUpdate(E_PQ_MEAS_TYPE measurement_type, std::map<QString, QVariant> optional_data);

protected:

   void processMeasurementRequest(T_DATAFRAME_SRVREQUEST request, std::map<QString, QVariant> metadata);
   void processClientError(const QString);
   void processUserBreakRequest();
   void sendStatusUpdate();


private:

   void setupTCSPC();
   void acquireSequence();
   void stopSequence();

   void acquisitionStatusChanged(bool acq_in_progress);

   ImageRenderWindow* flim_display = nullptr;

   QWidget* tcspc_control = nullptr;
   QMdiSubWindow* tcspc_settings_window = nullptr;
   FifoTcspc* tcspc = nullptr;

   std::shared_ptr<FlimFileWriter> file_writer;
   LifetimeDisplayWidget* preview_widget;
   FlimWorkspace* workspace;
   FlimServer* server;

   QTimer* status_timer;
};

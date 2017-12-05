#include "FlimReaderDataSource.h"
#include "LifetimeDisplayWidget.h"
#include "Cv3dUtils.h"
#include "FlimCubeWriter.h"

#include <memory>

FlimReaderDataSource::FlimReaderDataSource(const QString& filename_, QObject* parent) :
   FlimDataSource(parent)
{
   filename = filename_;
   reader = std::shared_ptr<FlimReader>(FlimReader::createReader(filename.toStdString()));

   reader->setTemporalResolution(8); // TODO

   worker = new DataSourceWorker(this);
   connect(this, &QObject::destroyed, worker, &QObject::deleteLater);

   int n_chan = reader->getNumChannels();
   count_rates_dummy.resize(n_chan);
}

FlimReaderDataSource::~FlimReaderDataSource()
{
   terminate = true;
   currently_reading = false;
   reader->stopReading();

   QMetaObject::invokeMethod(worker, "stop");

   std::lock_guard<std::mutex> lk(read_mutex);

   if (reader_thread.joinable())
      reader_thread.join();
}

cv::Mat FlimReaderDataSource::getIntensity()
{
   std::lock_guard<std::mutex> lk(image_mutex);
   return intensity;
};

cv::Mat FlimReaderDataSource::getMeanArrivalTime()
{
   std::lock_guard<std::mutex> lk(image_mutex);
   return mean_arrival_time;
};


void FlimReaderDataSource::update()
{
   if (terminate)
      return;

   if (task)
   {
      double progress = reader->getProgress();
      task->setProgress(progress);
   }

   {
      std::lock_guard<std::mutex> lk(read_mutex);

      if (!currently_reading) return;
      if (!data || !data->isReady()) return;

      int n_px = reader->numX() * reader->numY() * reader->numZ();
      int n_chan = reader->getNumChannels();
      int n_t = (int)data->timepoints.size();

      if (area(intensity) < n_px) return;

      cv::Mat intensitybuf = intensity.clone();
      cv::Mat marbuf = mean_arrival_time.clone();

      uint16_t* data_ptr = data->getDataPtr();
      float* intensity_ptr = (float*) intensitybuf.data;
      float* mean_arrival_time_ptr = (float*) marbuf.data;
      for (int p = 0; p < n_px; p++)
      {
         uint16_t I = 0;
         float It = 0;
         for (int c = 0; c < n_chan; c++)
            for (int t = 0; t < n_t; t++)
            {
               I += *data_ptr;
               It += (*data_ptr) * data->timepoints[t];

               data_ptr++;
            }
         intensity_ptr[p] = I;
         mean_arrival_time_ptr[p] = It / I;
      }
         
      // Apply intensity normalisation
      cv::Mat intensity_normalisation = reader->getIntensityNormalisation();
      if (!intensity_normalisation.empty())
      {
         cv::Mat norm;
         intensity_normalisation.convertTo(norm, CV_32F);
         norm += 0.1;
         cv::divide(intensitybuf, norm, intensitybuf);
         intensitybuf *= 100;
         std::cout << "Performing intensity normalisation\n";
      }

      std::lock_guard<std::mutex> lk_im(image_mutex);
      intensitybuf.copyTo(intensity);
      marbuf.copyTo(mean_arrival_time);
   }

   emit decayUpdated();
}

void FlimReaderDataSource::setupForRead()
{
   std::lock_guard<std::mutex> lk(read_mutex);

   connect(task.get(), &TaskProgress::cancelRequested, this, &FlimReaderDataSource::cancelRead);

   reader->clearStopSignal();
   
   int n_px = reader->dataSizePerChannel();
   int n_chan = reader->getNumChannels();
   int n_x = reader->numX();
   int n_y = reader->numY();
   int n_z = reader->numZ();

   {
      std::vector<int> dims = {n_z, n_y, n_x};
      std::lock_guard<std::mutex> lk(image_mutex);
      intensity = cv::Mat(dims, CV_32F, cv::Scalar(0));
      mean_arrival_time = cv::Mat(dims, CV_32F, cv::Scalar(0));
   }

   data = std::make_shared<FlimCube<uint16_t>>();
}

void FlimReaderDataSource::alignFrames()
{
   try
   {
      reader->alignFrames();
   }
   catch (std::runtime_error e)
   {
      emit error(e.what());
   }
}

void FlimReaderDataSource::readAlignedData()
{
   try
   {
      reader->readData(data);      
   }
   catch (std::runtime_error e)
   {
      emit error(e.what());
   }
}

void FlimReaderDataSource::cancelRead()
{
   reader->stopReading(); 
   terminate = true;
}


QWidget* FlimReaderDataSource::getWidget()
{
   auto widget = new LifetimeDisplayWidget;
   widget->setFlimDataSource(this);
   return widget;
}

void FlimReaderDataSource::saveData(const QString& root_name)
{
   auto tags = reader->getTags();
   auto reader_tags = reader->getReaderTags();
   auto images = reader->getImageMap();

   for (int z = 0; z<reader->numZ(); z++)
   {
      QString filename = QString("%1_%2.ffh").arg(root_name).arg(z,3);
      std::cout << "Filename: " << filename.toStdString() << "\n";
      FlimCubeWriter<uint16_t> writer(filename.toStdString(), data, z, tags, reader_tags, images);      
   }
}

void FlimReaderDataSource::savePreview(const QString& filename)
{
   writeScaledImage(filename.toStdString(), getIntensity());   
}
#include <PbMsgs/config.h>
#include <PbMsgs/Logger.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <functional>
#include <iostream>

#include <miniglog/logging.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>

namespace pb {

/** Copied from HAL/TicToc.h */
inline double RealTime() {
#if _POSIX_TIMERS > 0
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
#else
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + 1e-6 * (tv.tv_usec);
#endif
}

Logger& Logger::GetInstance() {
  static Logger s_instance;
  return s_instance;
}

Logger::Logger() : m_sFilename("proto.log"),
                   m_bShouldRun(false),
                   m_nMaxBufferSize(5000),
                   m_nMessagesWritten(0) {
}

Logger::~Logger() {
  StopLogging();
}

void Logger::ThreadFunc() {
  mode_t OpenMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int fd = open(m_sFilename.c_str(), O_RDWR | O_CREAT | O_TRUNC, OpenMode);

  if(fd == 0) {
    LOG(ERROR) << "Error opening file " << m_sFilename << std::endl;
    return;
  }

  google::protobuf::io::FileOutputStream raw_output(fd);
  raw_output.SetCloseOnDelete(true);
  google::protobuf::io::CodedOutputStream coded_output(&raw_output);

  ///-------------------- Write Magic Number %HAL
  const char magic_number[] = { '%', 'H', 'A', 'L' };
  coded_output.WriteRaw(magic_number,4);


  ///-------------------- Write Header Msg
  pb::Header hdr;
  hdr.set_version(PBMSGS_VERSION);
  hdr.set_date(RealTime());
  hdr.set_description("HAL Log File.");

  coded_output.WriteVarint32(hdr.ByteSize());

  if(!hdr.SerializeToCodedStream(&coded_output)) {
    LOG(FATAL) << "HAL: Failed to serialize HEADER to coded stream.";
  }

  ///-------------------- Run Logger
  while (m_bShouldRun) {
    {
      std::unique_lock<std::mutex> lock(m_QueueMutex);
      m_QueueCondition.wait(lock, [this]() {
          return !m_bShouldRun || !m_qMessages.empty();
        });

      if (!m_bShouldRun) break;
    }

    pb::Msg& msg = m_qMessages.front();
    const size_t size_bytes = msg.ByteSize();
    coded_output.WriteVarint32(size_bytes);

    if (!msg.IsInitialized()) {
      LOG(WARNING) << "Message is not initialized missing fields ("
                   << msg.InitializationErrorString() << "). Cannot serialize.";
    } else if(!msg.SerializeToCodedStream(&coded_output)) {
      LOG(WARNING) << "Failed to serialize to coded stream.";
    }

    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_qMessages.pop_front();
    ++m_nMessagesWritten;
  }

  LOG(INFO) << "Logger thread stopped. Wrote " << m_nMessagesWritten
            << " frames to " << m_sFilename << ".";
}

bool Logger::LogMessage(const pb::Msg &message) {
  if(!message.has_timestamp()){
    LOG(WARNING) << "Logging a message without a timestamp.";
  }

  if(!m_WriteThread.joinable()) {
    LogToFile(m_sFilename);
  }

  if(m_qMessages.size() >= m_nMaxBufferSize) {
    LOG(ERROR) << "Could not log message. Buffer is already at maximum size!";
    return false;
  }

  std::lock_guard<std::mutex> lock(m_QueueMutex);
  m_qMessages.push_back(message);
  m_QueueCondition.notify_one();
  return true;
}

void Logger::LogToFile(const std::string& filename) {
  LOG(INFO) << "Logger thread started...";
  StopLogging();

  m_nMessagesWritten = 0;
  m_sFilename = filename;
  m_bShouldRun = true;
  m_WriteThread = std::thread(&Logger::ThreadFunc, this);
}

std::string Logger::LogToFile(const std::string& sLogDir,
                              const std::string& sPrefix) {
  StopLogging();

  std::string sFileDir;
  int nCount = 0;
  while(1) {
    std::stringstream wss;
    wss << sLogDir << sPrefix << "_log" << nCount << ".log";
    std::ifstream ifile(wss.str());
    if(!ifile){
      sFileDir = wss.str();
      break;
    }
    nCount++;
  }

  LogToFile(sFileDir);
  return sFileDir;
}

void Logger::StopLogging() {
  if(m_WriteThread.joinable()) {
    m_bShouldRun = false;
    m_QueueCondition.notify_all();
    m_WriteThread.join();
  }
}

bool Logger::IsLogging() {
  return m_WriteThread.joinable();
}

void Logger::SetMaxBufferSize(unsigned int nBufferSize) {
  m_nMaxBufferSize = nBufferSize;
}

size_t Logger::buffer_size() const {
  return m_qMessages.size();
}

size_t Logger::messages_written() const {
  return m_nMessagesWritten;
}
}  // namespace pb

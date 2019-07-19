/**
 * ====================================================================
 * Author: Nikolaus Mayer, 2019 (mayern@cs.uni-freiburg.de)
 * ====================================================================
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/// For CPU usage statistics
#include <cstring>
#include <sys/times.h>
#include <sys/vtimes.h>

/// Local files
#include "fps.h"
#include "OptionParser.h"
#include "pacemaker.h"
#include "TextDecorator.h"
#include "Timer.h"



static std::vector<std::string> infilenames;
static std::vector<std::string> outfilenames;

/// Command-line options
optparse::Values options;


/**
 * Information about a system disk
 */
struct Disk {
  std::string name;
  size_t current_sectors_read;
  size_t last_sectors_read;
  size_t bytes_per_sector;
};


/**
 * A module to get information about the current disk I/O speeds
 */
struct DisksIOInfo {
  DisksIOInfo()
    : m_state{InfoState_t::INIT}
  {
    init();
  }

  void init()
  {
    /// Get disks info
    std::ifstream diskstats{"/proc/diskstats"};
    if (diskstats.bad() or not diskstats.is_open()) {
      m_state = InfoState_t::NO_DISKS_AVAILABLE;
      return;
    }
    while (not diskstats.eof()) {
      size_t dummy_int;
      size_t sectors_read;
      std::string disk_name;
      std::string dummy_string;

      /// Example:
      ///    8       4 sda4 5 0 28 108 0 0 0 0 0 108 108
      ///         NAME--^       ^--sectors_read
      diskstats >> dummy_int >> dummy_int >> disk_name
                >> dummy_int >> dummy_int >> sectors_read;
      std::getline(diskstats, dummy_string);
      if (diskstats.eof())
        break;

      /// Ignore "loopXXX" entries
      if (std::strncmp(disk_name.c_str(), "loop", std::strlen("loop")) == 0)
        continue;
    
      /// Get bytes-per-sector for this disk
      std::ifstream hw_sector_size{"/sys/block/" +
                                   disk_name +
                                   "/queue/hw_sector_size"};
      /// This file only exists for DISKS, not PARTITIONS
      if (hw_sector_size.bad() or not hw_sector_size.is_open())
        continue;

      size_t bytes_per_sector;
      hw_sector_size >> bytes_per_sector;
      hw_sector_size.close();
      
      m_disks.emplace_back(Disk{disk_name, sectors_read, 0, bytes_per_sector});
    }

    diskstats.close();

    m_state = InfoState_t::HAVE_DISKS;
  }

  void update()
  {
    if (m_state != InfoState_t::HAVE_DISKS) {
      std::cerr << "No disk I/O information available" << std::endl;
      return;
    }

    /// Get disks info
    std::ifstream diskstats{"/proc/diskstats"};
    if (diskstats.bad() or not diskstats.is_open()) {
      for (auto& disk : m_disks) {
        disk.current_sectors_read = 0;
        disk.last_sectors_read    = 0;
      }
    }
    while (not diskstats.eof()) {
      size_t dummy_int;
      size_t sectors_read;
      std::string disk_name;
      std::string dummy_string;

      /// Example:
      ///    8       4 sda4 5 0 28 108 0 0 0 0 0 108 108
      ///         NAME--^       ^--sectors_read
      diskstats >> dummy_int >> dummy_int >> disk_name
                >> dummy_int >> dummy_int >> sectors_read;
      std::getline(diskstats, dummy_string);
      if (diskstats.eof())
        break;

      for (auto& disk : m_disks) {
        if (disk.name == disk_name) {
          disk.last_sectors_read    = disk.current_sectors_read;
          disk.current_sectors_read = sectors_read;
        }
      }
    }
    diskstats.close();
  }

  size_t getFastestDiskRead() const
  {
    size_t fastest = 0;

    for (const auto& disk : m_disks) {
      const size_t read = disk.bytes_per_sector * 
                         (disk.current_sectors_read - disk.last_sectors_read);
      if (read > fastest)
        fastest = read;
    }
    return fastest;
  }


  enum class InfoState_t {
    INIT,
    HAVE_DISKS,
    NO_DISKS_AVAILABLE,
  };
  InfoState_t m_state;

  std::vector<Disk> m_disks;
};


/**
 * A module to get information about the current CPU usage
 * https://stackoverflow.com/a/64166
 */
struct CPUUsageInfo {
  CPUUsageInfo()
  {
    FILE* file;
    struct tms timeSample;
    char line[128];

    lastCPU = times(&timeSample);
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;

    file = fopen("/proc/cpuinfo", "r");
    numProcessors = 0;
    while(fgets(line, 128, file) != NULL){
      if (std::strncmp(line, "processor", 9) == 0) numProcessors++;
    }
    fclose(file);
  }

  float getTotalCPUUsage()
  {
    struct tms timeSample;
    clock_t now;
    float percent;

    now = times(&timeSample);
    if (now <= lastCPU or
        timeSample.tms_stime < lastSysCPU or
        timeSample.tms_utime < lastUserCPU) {
      //Overflow detection. Just skip this value.
      percent = -1.0;
    } else {
      percent = (timeSample.tms_stime - lastSysCPU) +
                (timeSample.tms_utime - lastUserCPU);
      percent /= (now - lastCPU);
      //percent /= numProcessors;
      //percent *= 100;
    }
    lastCPU = now;
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;

    return percent;
  }

  int getNumberOfCPUs() const
  {
    return numProcessors;
  }

  clock_t lastCPU, lastSysCPU, lastUserCPU;
  int numProcessors;
};


/**
 * A parallelizable data-reader
 */
struct Worker {
  Worker(const std::vector<int>& indices) 
    : m_indices{indices},
      m_status{WorkerStatus_t::INIT},
      m_workmode{WorkMode_t::ONLY_READ},
      m_done{0},
      m_worker_ID{s_running_workers_ID++}
  { }

  Worker(Worker&& rhs)
  {
    m_indices   = std::move(rhs.m_indices);
    m_status    = rhs.m_status;
    m_done      = rhs.m_done;
    m_worker_ID = rhs.m_worker_ID;
  }

  void Start()
  {
    if (m_status == WorkerStatus_t::RUNNING)
      return;
    m_status = WorkerStatus_t::RUNNING;
    m_thread_ptr = std::make_unique<std::thread>(&Worker::Loop, this);
  }

  void Stop() 
  {
    m_status = WorkerStatus_t::STOPPING;
    if (m_thread_ptr and m_thread_ptr->joinable())
      m_thread_ptr->join();
  }

  void Loop() 
  {
    for (const auto random_index : m_indices) {
      if (m_status != WorkerStatus_t::RUNNING) {
        m_status = WorkerStatus_t::FINISHED;
        return;
      }

      ++m_done;

      std::ifstream ifs;
      std::string content;
      std::ofstream ofs;

      if (m_workmode == WorkMode_t::ONLY_READ or
          m_workmode == WorkMode_t::READ_AND_WRITE) {
        /// Open random file

        ifs.open(infilenames[random_index], std::ifstream::binary);
        if (ifs.bad() or not ifs.is_open()) {
          std::cerr << "Cannot read" << infilenames[random_index] 
                    << std::endl;
          continue;
        }
        ifs.seekg(0, std::ios_base::end);
        const auto length{ifs.tellg()};
        ifs.seekg(0, std::ios_base::beg);
        content.resize(length);
        ifs.read((char*)&(content.c_str()[0]), length);

        //ifs.open(infilenames[random_index], std::ifstream::binary);
        //if (ifs.bad() or not ifs.is_open()) {
        //  std::cerr << "Cannot read" << infilenames[random_index] 
        //            << std::endl;
        //  continue;
        //}
        ///// Read entire file content
        //content = std::string{std::istreambuf_iterator<char>(ifs),
        //                      std::istreambuf_iterator<char>()};

        ifs.close();
      }
      if (m_workmode == WorkMode_t::ONLY_WRITE) {
        content.resize(std::stoi(options["write-size"]));

        ofs.open(outfilenames[random_index], std::ofstream::binary);
        if (ofs.bad() or not ofs.is_open()) {
          std::cerr << "Cannot write " << outfilenames[random_index] 
                    << std::endl;
          continue;
        }
        ofs.write(content.c_str(), content.size());
        ofs.close();
      }
      if (m_workmode == WorkMode_t::READ_AND_WRITE) {
        ofs.open(outfilenames[random_index], std::ofstream::binary);
        if (ofs.bad() or not ofs.is_open()) {
          std::cerr << "Cannot write " << outfilenames[random_index] 
                    << std::endl;
          continue;
        }
        ofs.write(content.c_str(), content.size());
        ofs.close();
      }
      
      /// Log data
      m_data_throughput_logger.AddSample(content.size());
    }

    m_status = WorkerStatus_t::FINISHED;
  }

  size_t getDoneCount() const
  {
    return m_done;
  }

  float getThroughput()
  {
    return m_data_throughput_logger.FPS(1.f);
  }

  size_t isDone() const
  {
    return (m_status == WorkerStatus_t::FINISHED);
  }

  enum class WorkerStatus_t {
    INIT,
    RUNNING,
    STOPPING,
    FINISHED,
  };

  enum class WorkMode_t {
    ONLY_READ,
    ONLY_WRITE,
    READ_AND_WRITE,
    DONT_DO_SHIT,
  };

  void setMode(WorkMode_t mode)
  {
    m_workmode = mode;
  }

  std::vector<int> m_indices;
  WorkerStatus_t m_status;
  WorkMode_t m_workmode;
  std::unique_ptr<std::thread> m_thread_ptr;
  size_t m_done;

  FramesPerSecond::FPSEstimator m_data_throughput_logger;

  int m_worker_ID;
  static int s_running_workers_ID;
};
/// Initialize static field
int Worker::s_running_workers_ID = 0;



/**
 * A simple statistics module
 */
struct Statistificator
{
  void addSample(float sample)
  {
    m_samples.push_back(sample);
  }

  /**
   * Mean
   */
  float average() const
  {
    float result{0.f};
    for (size_t i = 0; i < m_samples.size(); ++i) {
      result += m_samples[i];
    }
    return result/m_samples.size();
  }

  /**
   * Mean without 5% highest / lowest outliers
   */
  float robustAverage() const
  {
    if (m_samples.size() < 100) {
      std::cout << "! Statistificator only has " << m_samples.size() 
                << " samples which is not really enough for reliable results!"
                << std::endl;
    }

    /// Sort samples
    std::vector<float> tmp{m_samples};
    std::sort(tmp.begin(), tmp.end());

    /// Compute average, leaving out the lowest/highest 5%
    float result{0.f};
    size_t sample_count{0};
    for (size_t i = 0.05*tmp.size(); i < 0.95*tmp.size(); ++i) {
      result += tmp[i];
      ++sample_count;
    }

    return result / sample_count;
  }

  /**
   * Minimum
   */
  float min() const
  {
    float result{std::numeric_limits<float>::max()};
    for (auto v : m_samples)
      if (v < result)
        result = v;
    return result;
  }

  /**
   * Minimum, but ignore the first 2 values because those are often skewed by
   * program init overhead.
   */
  float robustMin() const
  {
    if (m_samples.size() < 3) {
      std::cerr << "Too few samples!" << std::endl;
      return -1.f;
    }

    float result{std::numeric_limits<float>::max()};
    for (size_t i = 2; i < m_samples.size(); ++i)
      if (m_samples[i] < result)
        result = m_samples[i];
    return result;
  }

  std::vector<float> m_samples;
};



/**
 * Wrap a string in a pretty gift box
 */
std::string Boxify(const std::string& content)
{
  size_t length{content.size()};
  std::ostringstream oss;
  oss << "╭"; for (size_t i = 0; i < length; ++i) oss << "─"; oss << "╮\n";
  oss << "│";                oss << content;                  oss << "│\n"; 
  oss << "╰"; for (size_t i = 0; i < length; ++i) oss << "─"; oss << "╯";
  return oss.str();
}



int main (int argc, char* argv[])
{
  /// Print prettification
  TextDecorator::TextDecorator TD{true, false};

  std::cout << Boxify("                              "
                      "iobench"
                      "                              ") << std::endl;

  /// Command line options
  optparse::OptionParser parser;
  parser.add_option("-i", "--infiles")
        .dest("infiles")
        .help("list of input filenames");
  parser.add_option("-o", "--outfiles")
        .dest("outfiles")
        .help("list of output filenames");
  parser.add_option("-j", "--jobs")
        .type("int")
        .set_default("1")
        .dest("jobs")
        .help("number of parallel workers to start");
  parser.add_option("-s", "--workload-split")
        .choices({"separate", "overlap", "same"})
        .set_default("separate")
        .dest("workload-split")
        .help("how files are split between workers ([\"separate\"] / \"overlap\" / \"same\")");
  parser.add_option("-r", "--randomize-files")
        .action("store_true")
        .set_default(false)
        .dest("randomize")
        .help("access listed files randomly instead of sequentially");
  parser.add_option("-m", "--mode")
        .choices({"read", "write", "readwrite"})
        .set_default("read")
        .dest("mode")
        .help("Benchmark mode ([\"read\"] / \"write\" / \"readwrite\")");
  parser.add_option("-w", "--write-size")
        .type("int")
        .set_default("1048576") /*1MiB*/
        .dest("write-size")
        .help("how many bytes to write per target file if --mode=\"write\"");
  options = parser.parse_args(argc, argv);


  /// Parse filenames for reading
  std::vector<int> file_indices;
  if (not options.is_set("infiles") and not options.is_set("outfiles")) {
    std::cerr << "Need at least one of [--infiles, --outfiles]" << std::endl;
    return EXIT_FAILURE;
  }
  if (options.is_set("infiles")) {
    std::ifstream infiles{options["infiles"]};
    if (infiles.bad() or not infiles.is_open()) {
      std::cerr << "Could not read list of inputs: " << options["infiles"] 
                << std::endl;
      return EXIT_FAILURE;
    }
    while (not infiles.eof()) {
      std::string tmp;
      infiles >> tmp;
      if (infiles.eof())
        break;
      infilenames.push_back(tmp);
    }

    if (options["mode"] == "write") {
      std::cout << "Ignoring --infiles because --mode=write is set" 
                << std::endl;
    }
  }
  if (options.is_set("outfiles")) {
    std::ifstream outfiles{options["outfiles"]};
    if (outfiles.bad() or not outfiles.is_open()) {
      std::cerr << "Could not read list of outputs: " << options["outfiles"] 
                << std::endl;
      return EXIT_FAILURE;
    }
    while (not outfiles.eof()) {
      std::string tmp;
      outfiles >> tmp;
      if (outfiles.eof())
        break;
      outfilenames.push_back(tmp);
    }

    if (options["mode"] == "read") {
      std::cout << "Ignoring --outfiles because --mode=read is set" 
                << std::endl;
    }
  }
  /// Generate list of indices to files
  for (size_t i = 0; i < std::max(infilenames.size(), 
                                  outfilenames.size()); ++i)
    file_indices.push_back(i);


  std::cout << "Parsed " << TD.bold(file_indices.size()) << " entries."
            << std::endl;

  auto RNG = std::default_random_engine{std::random_device{}()};

  /// Randomly shuffle the list of all filenames
  if (options.get("randomize")) {
    std::cout << "Randomizing filenames" << std::endl;
    std::shuffle(file_indices.begin(), file_indices.end(), RNG);
  }

  /// Create workers
  std::vector<Worker> workers;
  const size_t num_workers{static_cast<size_t>(std::stoi(options["jobs"]))};
  std::cout << "Spawning " << num_workers << " worker threads..." << std::endl;
  if (options["workload-split"] == "separate") {
    /// Distribute work equally among all workers
    std::cout << "Workload will be equally distributed among all workers."
              << std::endl;
    const size_t slice_size{file_indices.size() / num_workers};
    for (size_t i = 0; i < num_workers; ++i) {
      size_t slice_point_a{slice_size * i};
      size_t slice_point_b{std::min(slice_size * (i + 1) - 1,
                                    file_indices.size())};
      std::vector<int> slice(file_indices.begin() + slice_point_a,
                             file_indices.begin() + slice_point_b);
      workers.emplace_back(std::move(Worker{slice}));
    }
  } else if (options["workload-split"] == "overlap") {
    /// All workers use the same data, but each worker uses an individual
    /// randomized sequence
    std::cout << "Workload is the same for all workers, but random for each."
              << std::endl;
    std::vector<int> copy{file_indices};
    std::shuffle(copy.begin(), copy.end(), RNG);
    workers.emplace_back(std::move(Worker{copy}));
  } else if (options["workload-split"] == "same") {
    /// All workers use the same data sequence
    std::cout << "Workload is exactly the same for all workers." << std::endl;
    for (size_t i = 0; i < num_workers; ++i) {
      workers.emplace_back(std::move(Worker{file_indices}));
    }
  } else {
    std::cerr << "Unhandled choice for \"workload-split\"" << std::endl;
    return EXIT_FAILURE;
  }
 
  /// Start workers
  for (auto& w : workers) {
    if (options["mode"] == "read") {
      w.setMode(Worker::WorkMode_t::ONLY_READ);
    } else if (options["mode"] == "write") {
      w.setMode(Worker::WorkMode_t::ONLY_WRITE);
    } else if (options["mode"] == "readwrite") {
      w.setMode(Worker::WorkMode_t::READ_AND_WRITE);
    } else {
      std::cerr << "Unhandled choice for \"mode\"" << std::endl;
      return EXIT_FAILURE;
    }

    w.Start();
  }


  /**
   * returns true IFF all workers are done
   */
  auto allWorkersFinished = [&workers]() -> bool {
    return std::all_of(workers.begin(),
                       workers.end(),
                       [](const auto& worker) { return worker.isDone(); });
  };


  /// Info about CPU usage
  CPUUsageInfo cpu_info;
  /// Info about actual disk I/O speeds
  DisksIOInfo disks_info;
  /// Print frequency
  Pacemaker::Pacemaker print_timer{1.f};
  /// Simple data statistics
  Statistificator read_speed_log;
  /// Log execution time
  Timer::Timer benchmark_time{false};

  /**
   * Print a horizontal "-----" line
   */
  auto PrintHline = []() {
    for (size_t i = 0; i < 10; ++i)
      std::cout << "--------";
    std::cout << std::endl;
  };

  /**
   * Print column names
   */
  auto PrintHeaders = []() {
    std::cout << "Progress\t"
              << "speed\t\t"
              << "speed\t\t"
              << "CPU usage\t"
              << "CPU usage\t"
              << std::endl;
    std::cout << "\t\t"
              << "(total)\t\t"
              << "(per worker)\t"
              << "(total)\t\t"
              << "(per worker)\t"
              << std::endl;
  };

  /// Output header
  PrintHline();
  PrintHeaders();
  PrintHline();


  while (not allWorkersFinished()) {

    /// Print info or sleep
    if (print_timer.IsDue()) {


      /// Get progress and throughput per worker
      float done_sum{0.f};
      float throughput_sum{0.f};
      size_t active_workers{0};
      for (auto& worker : workers) {
        done_sum += worker.getDoneCount();
        throughput_sum += worker.getThroughput();
        if (not worker.isDone())
          ++active_workers;
      }
      if (options["workload-split"] == "overlap" or
          options["workload-split"] == "same") {
        done_sum /= num_workers;
      }

      read_speed_log.addSample(throughput_sum);

      const float cpu_usage{cpu_info.getTotalCPUUsage()};

      /// Bleh, ugly hack. My TextDecorator does not play well with the
      /// iomanip things, so we have to pre-format the number.
      std::ostringstream oss;
      oss << std::setw(7) << std::setprecision(1) << std::fixed
          << throughput_sum / (1024*1024);

      std::cout << std::setw(7) << std::setprecision(2) << std::fixed
                << static_cast<float>(100*done_sum)/infilenames.size() << "%\t"
                << TD.bold(oss.str() + " MB/s") << "\t"
                << std::setw(7) << std::setprecision(1) << std::fixed
                << throughput_sum / (1024*1024) / active_workers << " MB/s\t"
                << std::setw(7) << std::setprecision(1) << std::fixed
                << cpu_usage*100 << "%\t"
                << std::setw(7) << std::setprecision(1) << std::fixed
                << cpu_usage*100/active_workers << "%\t" << std::endl;

      /// Check if benchmarking is constrained by CPU (which would be bad)
      //if (cpu_usage >= 0.9*cpu_info.getNumberOfCPUs()) {
      if (cpu_usage >= 0.9*active_workers) {
        std::cout << "     " << TD.red(TD.bold("!!!")) << " " 
                  << "(benchmark might be CPU-constrained; use more workers!)"
                  << std::endl;
      }
      /// Check if experienced read speed is higher than actual disk read
      /// (indicates that data is fetched from some cache)
      disks_info.update();
      const size_t actual_disk_speed{disks_info.getFastestDiskRead()};
      if (throughput_sum > 1.1 * actual_disk_speed) {
        std::cout << "     " << TD.red(TD.bold("!!!")) << " " 
                  << "(actual disk reading is much slower ("
                  << actual_disk_speed / (1024*1024) << "MB/s); "
                  << "data may be cached!)"
                  << std::endl;
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  /// UX 101: If you have a progress indicator, make sure it shows "100%"
  std::cout << " 100.00%" << std::endl;
  PrintHline();
  PrintHeaders();
  PrintHline();

  /// Print some statistics
  std::cout << "Total execution time: " 
            << benchmark_time.ElapsedSeconds() << " seconds"
            << std::endl;
  const float avg_read_speed{read_speed_log.robustAverage()/(1024*1024)};
  std::cout << "Average cumulative reading speed: " 
            << TD.red(TD.bold(avg_read_speed)) << TD.red(TD.bold(" MB/s"))
            << std::endl;
  const float min_read_speed{read_speed_log.robustMin()/(1024*1024)};
  std::cout << "Minimum cumulative reading speed: " 
            << TD.red(TD.bold(min_read_speed)) << TD.red(TD.bold(" MB/s"))
            << std::endl;
            
                    

  /// Stop workers
  for (auto& w : workers)
    w.Stop();
  
  return EXIT_SUCCESS;
}


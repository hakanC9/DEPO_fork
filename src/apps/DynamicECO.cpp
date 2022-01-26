#include "../Eco.hpp"

#include "../helpers/results_container.hpp"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

inline std::pair<TargetMetric, SearchType>
parseArgs(po::variables_map& map)
{
    auto search = SearchType::LINEAR_SEARCH;
    auto metric = TargetMetric::MIN_E;
    if (map.count("gss"))
    {
        map.erase("gss");
        std::cout << "Using Golden Section Search algorithm as selected.\n";
        search = SearchType::GOLDEN_SECTION_SEARCH;
    }
    else if (map.count("ls"))
    {
        map.erase("ls");
        std::cout << "Using Linear Search algorithm as selected.\n";
        search = SearchType::LINEAR_SEARCH;
    }
    else
    {
        std::cout << "Using Linear Search algorithm by default.\n";
    }
    if (map.count("en"))
    {
        map.erase("en");
        std::cout << "Using ENERGY metric as selected.\n";
        metric = TargetMetric::MIN_E;
    }
    else if (map.count("edp"))
    {
        map.erase("edp");
        std::cout << "Using ENERGY DELAY PRODUCT metric as selected.\n";
        metric = TargetMetric::MIN_E_X_T;
    }
    else if (map.count("eds"))
    {
        map.erase("eds");
        std::cout << "Using ENERGY DELAY SUM metric as selected.\n";
        metric = TargetMetric::MIN_M_PLUS;
    }
    else
    {
        std::cout << "Using ENERGY metric by default.\n";
    }
    return std::make_pair(metric, search);
}

void cleanArgv(int& argc, char* argv[])
{
    for (int idx = 1; idx < argc;)
    {
        const auto flag = std::string(argv[idx]);
        if (flag == "--ls" || flag == "--gss" || flag == "--en" || flag == "--edp" || flag == "--eds")
        {
            for (int i = 1; i < argc -1; i++)
            {
                argv[i] = argv[i+1];
            }
            argv[argc-1] = nullptr;
            argc--;
        }
        else
        {
            idx++;
        }
    }
}

int main (int argc, char *argv[]) {

    // temporary fix
    std::ofstream watchdog ("/proc/sys/kernel/nmi_watchdog", std::ios::out | std::ios::trunc);
	watchdog << "0";
	watchdog.close();

    auto search = SearchType::LINEAR_SEARCH;
    auto metric = TargetMetric::MIN_E;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("gss", "use Golden Section Search algorithm")
        ("ls", "use Linear search algorithm")
        ("en", "use Energy metric")
        ("edp", "use Energy Delay Product metric")
        ("eds", "use Energy Delay Sum metric")
    ;
    po::variables_map optionsMap;
    po::store(po::parse_command_line(argc, argv, desc), optionsMap);
    po::notify(optionsMap);
    if (optionsMap.count("help"))
    {
        std::cout << desc << "\n";
        return 1;
    }
    std::tie(metric, search) = parseArgs(optionsMap);
    cleanArgv(argc, argv);

    Eco eco;
    std::ofstream outResultFile (eco.getResultFileName(), std::ios::out | std::ios::trunc);

    BothStream bout(outResultFile);
    bout << "# ";
    for (int i=1; i<argc; i++) {
        bout <<  argv[i] << " ";
    }
    bout << "\n" << argc;

    FinalPowerAndPerfResult result;    result = eco.runAppWithSearch(argv, metric, search);
    std::cout << "\n"
              << "E: " << result.energy << " J\n"
              << "t: " << result.time_.totalTime_ << " s\n"
              << "P: " << result.pkgPower << " W\n";

    outResultFile.close();
    eco.plotPowerLog();

	return 0;
}
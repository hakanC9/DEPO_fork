#include "../Eco.hpp"

#include "../helpers/results_container.hpp"

inline void removeParsedArg(int& argc, char** argv)
{
    for (int i = 1; i < argc -1; i++)
    {
        argv[i] = argv[i+1];
    }
    argc--;
}
int main (int argc, char *argv[]) {

    // temporary fix
    std::ofstream watchdog ("/proc/sys/kernel/nmi_watchdog", std::ios::out | std::ios::trunc);
	watchdog << "0";
	watchdog.close();

    Eco eco;
    std::ofstream outResultFile (eco.getResultFileName(), std::ios::out | std::ios::trunc);

    BothStream bout(outResultFile);
    bout << "# ";
    for (int i=1; i<argc; i++) {
        bout << argv[i] << " ";
    }
    bout << "\n";

    FinalPowerAndPerfResult result;
    //TODO: handle parsing args - for now one must set the desired target metric and search algorithm below
    auto search = SearchType::LINEAR_SEARCH;
    auto metric = TargetMetric::MIN_E;
    result = eco.runAppWithSearch(argv, metric, search);
    // result = eco.runAppWithSampling(argv);
    std::cout << "\n"
              << "E: " << result.energy << " J\n"
              << "t: " << result.time_.totalTime_ << " s\n"
              << "P: " << result.pkgPower << " W\n";

    outResultFile.close();
    eco.plotPowerLog();

	return 0;
}
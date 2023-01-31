/*
   Copyright 2022, Adam Krzywaniak.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "../Eco.hpp"
#include "../plot_builder.hpp"

int main (int argc, char *argv[]) {

    // temporary fix
    std::ofstream watchdog ("/proc/sys/kernel/nmi_watchdog", std::ios::out | std::ios::trunc);
	watchdog << "0";
	watchdog.close();

    std::unique_ptr<EcoApi> eco;
    eco = std::make_unique<Eco>();

    std::ofstream outResultFile (eco->getResultFileName(), std::ios::out | std::ios::trunc);

    BothStream bout(outResultFile);
    bout << "# ";
    for (int i=1; i<argc; i++) {
        bout << argv[i] << " ";
    }
    bout << "\n";
    bout << "#Pow.cap\tEnerg\tAv.P.PK"/*\tAv.P.P0\tAv.P.P1\tAv.P.DR*/<<"\ttime\tExt"
         << "\tdE\tdt\t%dE\t%dt"/*\tdE/dt\t%dE/%dt\tinstr\tcycl\tins/s\tcyc/s*/<<"\tP/(cycl/s)\n";
    bout << "#[W]\t[J]\t[W]"/*\t[W]\t[W]\t[W]*/ << "\t[s]"
         << "\t[Js]\t[J]\t[s]\t[%J]\t[%s]"/*\t[J/s]\t[-]\t[x1M]\t[x1M]\t[x1M/s]\t[x1M/s]\t*/ <<"[(cycl)/J]\t[(cycl/s)^2/W)]\n";


    auto domainVec = {PowerCapDomain::PKG};//,
                    //   PowerCapDomain::PP0,
                    //   PowerCapDomain::PP1,
                    //   PowerCapDomain::DRAM};
    dynamic_cast<Eco*>(eco.get())->referenceRunWithoutCaps(argv);
    for (auto currentDom : domainVec) {
        dynamic_cast<Eco*>(eco.get())->runAppForEachPowercap(argv, bout, currentDom);
        sleep(1);
    }

    bout.flush();
    outResultFile.close();
    eco->plotPowerLog();

    // Plot result file automatically
    std::string imgFileName = eco->getResultFileName();
    PlotBuilder p(imgFileName.replace(imgFileName.end() - 4,
                                      imgFileName.end(),
                                      "_Et.png"));
	// p.setPlotTitle(eco->getCpuName());
    std::cout << "Processing " << eco->getResultFileName() << " file...\n";
	p.plotEPet(eco->getResultFileName());
    p.submitPlot();

    // Plot result file automatically
    std::string imgFileName2 = eco->getResultFileName();
    PlotBuilder p2(imgFileName2.replace(imgFileName2.end() - 3,
                                      imgFileName2.end(),
                                      "png"));
	// p2.setPlotTitle(eco->getCpuName());
    std::cout << "Processing " << eco->getResultFileName() << " file...\n";
	p2.plotEPall(eco->getResultFileName());
    p2.submitPlot();

	return 0;
}

#include "MatPowerPlugin.h"
#include "WindowPlugin.h"
#include <sc/IPlugin.h>
#include <fo/FileOperations.h>
#include <mu/ScopedCLocale.h>
#include <td/StringUtils.h>
#include <cnt/PushBackVector.h>
#include <sparse/IMatrix.h>

#include <map>
#include <set>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdarg>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  Pomocne funkcije
// ============================================================

static std::string sfmt(const char* f, ...)
{
    char buf[512];
    va_list args;
    va_start(args, f);
    vsnprintf(buf, sizeof(buf), f, args);
    va_end(args);
    return std::string(buf);
}

static std::string trimStr(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Evaluira jednostavne MATPOWER izraze: broj, a/b, Inf
static double evalExpr(const std::string& raw)
{
    std::string s = trimStr(raw);
    if (s.empty()) return 0.0;
    if (s == "Inf" || s == "inf" || s == "+Inf") return 1e100;
    if (s == "-Inf" || s == "-inf")               return -1e100;

    for (int i = 1; i < (int)s.size(); i++)
    {
        if (s[i] == '/' && s[i-1] != 'e' && s[i-1] != 'E')
        {
            try
            {
                double num = std::stod(s.substr(0, i));
                double den = std::stod(s.substr(i + 1));
                return (std::abs(den) > 1e-15) ? num / den : 0.0;
            }
            catch (...) { break; }
        }
    }
    try   { return std::stod(s); }
    catch (...) { return 0.0; }
}

// ============================================================
//  Strukture podataka
// ============================================================

struct MpcBus {
    int    id, type;   // type: 1=PQ, 2=PV, 3=Slack
    double Pd, Qd;
    double Gs, Bs;
    double Vm, Va;
};

struct MpcGen {
    int    busId, status;
    double Pg, Qg, Vg;
};

struct MpcBranch {
    int    from, to, status;
    double r, x, b;
    double tap, shift;
};

struct MpcData {
    double                 baseMVA = 100.0;
    std::vector<MpcBus>    buses;
    std::vector<MpcGen>    gens;
    std::vector<MpcBranch> branches;
};

// ============================================================
//  Parser MATPOWER .m case fajla
// ============================================================

static bool parseMFile(const td::String& path, MpcData& data, td::String& errMsg)
{
    mu::ScopedCLocale locale;

    fo::InFile inFile;
    if (!fo::openExistingBinaryFile(inFile, path))
    {
        errMsg = "ERROR! Cannot open .m file!";
        return false;
    }

    enum class Block { None, Bus, Gen, Branch };
    Block block = Block::None;

    fo::LineNormal buffer;
    cnt::PushBackVector<td::String> tokens;
    tokens.reserve(20);

    while (fo::getLine(inFile, buffer))
    {
        std::string s(buffer.c_str());

        // Ukloni komentar
        auto pct = s.find('%');
        if (pct != std::string::npos) s = s.substr(0, pct);
        s = trimStr(s);
        if (s.empty()) continue;

        // Kraj bloka
        if (block != Block::None &&
            (s.find("];") != std::string::npos || s == "]"))
        { block = Block::None; continue; }

        // Rani izlaz ako su sve tri matrice procitane
        if (block == Block::None &&
            !data.buses.empty() && !data.gens.empty() && !data.branches.empty())
            break;

        // baseMVA
        if (block == Block::None && s.find("mpc.baseMVA") == 0)
        {
            auto eq = s.find('=');
            if (eq != std::string::npos)
            {
                std::string val = s.substr(eq + 1);
                val.erase(std::remove(val.begin(), val.end(), ';'), val.end());
                data.baseMVA = evalExpr(val);
            }
            continue;
        }

        // Pocetak matrica
        if (block == Block::None && s.find('[') != std::string::npos)
        {
            if (s.find("mpc.bus") == 0)
                { block = Block::Bus;    continue; }
            if (s.find("mpc.gen") == 0 && s.find("mpc.gencost") == std::string::npos)
                { block = Block::Gen;    continue; }
            if (s.find("mpc.branch") == 0)
                { block = Block::Branch; continue; }
        }

        if (block == Block::None) continue;

        // Tokenizacija reda pomocju natID PushBackVector
        tokens.reset();
        td::String tdRow(s.c_str());
        tdRow.split(" \t", tokens);

        std::vector<double> row;
        row.reserve(tokens.size());
        for (size_t k = 0; k < tokens.size(); ++k)
        {
            if (!tokens[k].isEmpty())
                row.push_back(evalExpr(std::string(tokens[k].c_str())));
        }
        if (row.empty()) continue;

        if (block == Block::Bus && row.size() >= 9)
        {
            MpcBus b;
            b.id = (int)std::round(row[0]);  b.type = (int)std::round(row[1]);
            b.Pd = row[2];  b.Qd = row[3];
            b.Gs = row[4];  b.Bs = row[5];
            b.Vm = row[7];  b.Va = row[8];
            data.buses.push_back(b);
        }
        else if (block == Block::Gen && row.size() >= 8)
        {
            MpcGen g;
            g.busId  = (int)std::round(row[0]);
            g.Pg = row[1];  g.Qg = row[2];  g.Vg = row[5];
            g.status = (int)std::round(row[7]);
            if (g.status == 1) data.gens.push_back(g);
        }
        else if (block == Block::Branch && row.size() >= 11)
        {
            MpcBranch br;
            br.from = (int)std::round(row[0]);  br.to = (int)std::round(row[1]);
            br.r = row[2];  br.x = row[3];  br.b = row[4];
            br.tap = row[8];  br.shift = row[9];
            br.status = (int)std::round(row[10]);
            if (br.status == 1) data.branches.push_back(br);
        }
    }

    if (data.buses.empty())    { errMsg = "ERROR! No bus data in .m file!";    return false; }
    if (data.branches.empty()) { errMsg = "ERROR! No branch data in .m file!"; return false; }
    return true;
}

// ============================================================
//  Gradnja admitansne matrice Y - koristi sparse::ICmplxMatrix
// ============================================================

static void buildY(const MpcData& data,
                   const std::map<int,int>& id2idx,
                   std::vector<td::cmplx>& Yii,
                   std::map<std::pair<int,int>, td::cmplx>& Yij,
                   std::vector<std::set<int>>& neighbors,
                   sparse::ICmplxMatrix* pYbus)
{
    for (auto& br : data.branches)
    {
        auto iF = id2idx.find(br.from), iT = id2idx.find(br.to);
        if (iF == id2idx.end() || iT == id2idx.end()) continue;
        int fi = iF->second, ti = iT->second;

        double tap = (std::abs(br.tap) < 1e-10) ? 1.0 : br.tap;
        double shiftRad = br.shift * M_PI / 180.0;

        td::cmplx y_s = (std::abs(br.r) > 1e-15 || std::abs(br.x) > 1e-15)
            ? td::cmplx(1.0, 0.0) / td::cmplx(br.r, br.x)
            : td::cmplx(0.0, 0.0);

        td::cmplx y_b(0.0, br.b / 2.0);
        td::cmplx a = tap * std::exp(td::cmplx(0.0, shiftRad));
        double    aa = (a * std::conj(a)).real();  // |a|^2

        // Pi model transformatora
        Yii[fi] += (y_s + y_b) / aa;
        Yii[ti] += y_s + y_b;
        Yij[{fi, ti}] += -y_s / std::conj(a);
        Yij[{ti, fi}] += -y_s / a;

        neighbors[fi].insert(ti);
        neighbors[ti].insert(fi);
    }

    // Paralelne admitanse iz podataka o cvorovima
    for (int idx = 0; idx < (int)data.buses.size(); idx++)
    {
        Yii[idx] += td::cmplx(data.buses[idx].Gs / data.baseMVA,
                               data.buses[idx].Bs / data.baseMVA);
    }

    // Popunjavanje sparse::ICmplxMatrix (indeksi 1-based)
    int n = (int)data.buses.size();
    for (int idx = 0; idx < n; idx++)
        pYbus->addTriple1(idx + 1, idx + 1, Yii[idx]);
    for (auto& kv : Yij)
        pYbus->addTriple1(kv.first.first + 1, kv.first.second + 1, kv.second);
}

// ============================================================
//  Pisanje .dmodl u pravougaonim koordinatama
// ============================================================

static bool writeDmodl(const td::String& outPath,
                       const MpcData& data,
                       const std::map<int,int>& id2idx,
                       const std::vector<td::cmplx>& Yii,
                       const std::map<std::pair<int,int>, td::cmplx>& Yij,
                       const std::vector<std::set<int>>& neighbors,
                       const std::vector<double>& Pinj,
                       const std::vector<double>& Qinj,
                       sc::IPlugin* pIPlugin,
                       const Options& options,
                       td::String& errMsg)
{
    mu::ScopedCLocale locale;

    int n = (int)data.buses.size();
    std::vector<int> idx2id(n);
    for (auto& p : id2idx) idx2id[p.second] = p.first;

    std::set<int> slackIds, pvIds;
    for (auto& b : data.buses)
    {
        if (b.type == 3) slackIds.insert(b.id);
        else if (b.type == 2) pvIds.insert(b.id);
    }

    std::map<int, double> pvVsp;
    for (auto& g : data.gens) pvVsp[g.busId] = g.Vg;

    std::ostringstream out;
    out.precision(10);

    // Header
    out << sfmt("Header:\n\tmaxIter=%d\n\treport=Solved\n\tmaxReps=-1\n\toutToTxt=false\nend\n",
                options.maxIter);
    out << "// Generated by MatPower Converter Plugin - rectangular coordinates\n";
    out << sfmt("Model [type=NL domain=real eps=%g name=\"%s\"]:\n",
                options.eps, options.modelName.c_str());

    // Vars
    out << "Vars [out=true]:\n";
    for (int idx = 0; idx < n; idx++)
    {
        int bid = idx2id[idx];
        if (slackIds.count(bid)) continue;
        auto& bus = data.buses[idx];
        double vaRad = bus.Va * M_PI / 180.0;
        out << sfmt("\te_%d = %.10g; f_%d = %.10g\n",
                    bid, bus.Vm * std::cos(vaRad),
                    bid, bus.Vm * std::sin(vaRad));
    }

    // Params
    out << "Params:\n";

    // Slack - fiksne vrijednosti
    for (auto& bus : data.buses)
    {
        if (!slackIds.count(bus.id)) continue;
        double vaRad = bus.Va * M_PI / 180.0;
        out << sfmt("\te_%d = %.10g [out=true]; f_%d = %.10g [out=true]\n",
                    bus.id, bus.Vm * std::cos(vaRad),
                    bus.id, bus.Vm * std::sin(vaRad));
    }

    // G i B elementi Y matrice
    for (int idx = 0; idx < n; idx++)
    {
        int bid = idx2id[idx];
        out << sfmt("\tG_%d_%d = %.10g; B_%d_%d = %.10g\n",
                    bid, bid, Yii[idx].real(),
                    bid, bid, Yii[idx].imag());
        for (int nIdx : neighbors[idx])
        {
            int nBid = idx2id[nIdx];
            auto it = Yij.find({idx, nIdx});
            if (it == Yij.end()) continue;
            out << sfmt("\tG_%d_%d = %.10g; B_%d_%d = %.10g\n",
                        bid, nBid, it->second.real(),
                        bid, nBid, it->second.imag());
        }
    }

    // Injekcije snaga
    for (int idx = 0; idx < n; idx++)
    {
        int bid = idx2id[idx];
        if (slackIds.count(bid)) continue;
        bool hasPQ = (std::abs(Pinj[idx]) > 1e-15 || std::abs(Qinj[idx]) > 1e-15);
        if (pvIds.count(bid))
        {
            out << sfmt("\tP_%d = %.10g\n", bid, Pinj[idx]);
            double vsp = pvVsp.count(bid) ? pvVsp.at(bid) : data.buses[idx].Vm;
            out << sfmt("\tVsp2_%d = %.10g\n", bid, vsp * vsp);
        }
        else if (hasPQ)
        {
            out << sfmt("\tP_%d = %.10g; Q_%d = %.10g\n",
                        bid, Pinj[idx], bid, Qinj[idx]);
        }
    }

    // NLEs
    out << "NLEs:\n";
    for (int idx = 0; idx < n; idx++)
    {
        int bid = idx2id[idx];
        if (slackIds.count(bid)) continue;

        bool isPV  = pvIds.count(bid) > 0;
        bool hasPQ = (std::abs(Pinj[idx]) > 1e-15 || std::abs(Qinj[idx]) > 1e-15);

        // P jednacina
        {
            std::string eq = sfmt("\tG_%d_%d*(e_%d^2+f_%d^2)", bid, bid, bid, bid);
            for (int nIdx : neighbors[idx])
            {
                int nBid = idx2id[nIdx];
                eq += sfmt(" + G_%d_%d*(e_%d*e_%d+f_%d*f_%d)"
                           " - B_%d_%d*(e_%d*f_%d-f_%d*e_%d)",
                           bid, nBid, bid, nBid, bid, nBid,
                           bid, nBid, bid, nBid, bid, nBid);
            }
            if (!hasPQ && !isPV) eq += " = 0\n";
            else                 eq += sfmt(" = P_%d\n", bid);
            out << eq;
        }

        if (isPV)
        {
            // Naponska referenca: e_i^2 + f_i^2 = Vsp_i^2
            out << sfmt("\te_%d^2 + f_%d^2 = Vsp2_%d\n", bid, bid, bid);
        }
        else
        {
            // Q jednacina
            std::string qeq = sfmt("\t-B_%d_%d*(e_%d^2+f_%d^2)", bid, bid, bid, bid);
            for (int nIdx : neighbors[idx])
            {
                int nBid = idx2id[nIdx];
                qeq += sfmt(" + G_%d_%d*(f_%d*e_%d-e_%d*f_%d)"
                            " - B_%d_%d*(e_%d*e_%d+f_%d*f_%d)",
                            bid, nBid, bid, nBid, bid, nBid,
                            bid, nBid, bid, nBid, bid, nBid);
            }
            if (!hasPQ) qeq += " = 0\n";
            else        qeq += sfmt(" = Q_%d\n", bid);
            out << qeq;
        }
    }

    out << "end\n";

    // Zapis u memorijski arhiv i fajl (identican EQPlugin obrascu)
    std::string content = out.str();
    auto pDigitModel = pIPlugin->getArchive(sc::IPlugin::ArchType::DigitalModel);
    auto& memDigitalOut = *pDigitModel;
    memDigitalOut.put(content.c_str(), content.size());

    std::ofstream fDigital;
    if (!fo::createTextFile(fDigital, outPath))
    {
        errMsg = "ERROR! Cannot create output file!";
        return false;
    }
    memDigitalOut.writeToFile(fDigital);
    fDigital.close();

    errMsg = "OK! Model successfully created!";
    return true;
}

// ============================================================
//  Javna funkcija createModel
// ============================================================

bool createModel(const td::String& inputFileName,
                 const td::String& outFileName,
                 sc::IPlugin* pIPlugin,
                 const Options& options,
                 gui::LineEdit& status)
{
    mu::ScopedCLocale locale;

    MpcData data;
    td::String errMsg;
    if (!parseMFile(inputFileName, data, errMsg)) { status = errMsg; return false; }

    int n = (int)data.buses.size();
    std::map<int,int> id2idx;
    for (int i = 0; i < n; i++) id2idx[data.buses[i].id] = i;

    std::vector<td::cmplx> Yii(n, td::cmplx(0.0, 0.0));
    std::map<std::pair<int,int>, td::cmplx> Yij;
    std::vector<std::set<int>> neighbors(n);

    // Procijenjeni broj nenultih elemenata: dijagonala + 2 po grani
    int estimatedNZ = n + 2 * (int)data.branches.size();
    sparse::CmplxMatrixReleaser Ybus(sparse::createCmplxMatrix(n, n, estimatedNZ));

    buildY(data, id2idx, Yii, Yij, neighbors, Ybus.ptr());

    std::vector<double> Pinj(n, 0.0), Qinj(n, 0.0);
    for (int i = 0; i < n; i++)
    {
        Pinj[i] -= data.buses[i].Pd / data.baseMVA;
        Qinj[i] -= data.buses[i].Qd / data.baseMVA;
    }
    for (auto& g : data.gens)
    {
        auto it = id2idx.find(g.busId);
        if (it == id2idx.end()) continue;
        Pinj[it->second] += g.Pg / data.baseMVA;
        Qinj[it->second] += g.Qg / data.baseMVA;
    }

    bool ok = writeDmodl(outFileName, data, id2idx, Yii, Yij, neighbors,
                         Pinj, Qinj, pIPlugin, options, errMsg);
    status = errMsg;
    return ok;
}

// ============================================================
//  Plugin klasa — identican EQPlugin obrascu
// ============================================================

class Plugin : public sc::IPlugin
{
    MemoryArchiveContainer _outArchives;
    WindowPlugin* _pWnd = nullptr;

public:
    Plugin()
    {
        //dont change this
        for (size_t i = 0; i < size_t(ArchType::NA); ++i)
            _outArchives[i] = nullptr;
    }

    void show(gui::Window* parentWnd,
              MemoryArchiveContainer& archives,
              td::UINT4 wndID,
              const sc::IPlugin::Cleaner& cleaner,
              const sc::IPlugin::CallBack& onComplete) override final
    {
        //dont change this
        for (size_t i = 0; i < size_t(ArchType::NA); ++i)
            _outArchives[i] = archives[i];

        if (_pWnd)
            _pWnd->setFocus();
        else
        {
            _pWnd = new WindowPlugin(parentWnd, this, onComplete, cleaner, wndID);
            _pWnd->open();
        }
    }

    td::String getMenuName() const override final
    {
        return "RctPlugin";
    }

    arch::MemoryOut* getArchive(sc::IPlugin::ArchType type) override final
    {
        //dont change this
        auto iType = size_t(type);
        if (iType >= getMaxSupportedArchiveParts())
            return nullptr;
        return _outArchives[iType];
    }

    MemoryArchiveContainer& getArchives() override final
    {
        //dont change this
        return _outArchives;
    }

    td::String getOutFileName() const override final
    {
        //dont change this
        assert(_pWnd);
        return _pWnd->getOutFileName();
    }

    size_t getMaxSupportedArchiveParts() const override final
    {
        return size_t(ArchType::NA); //don't change this
    }

    ModelType getModelType() const override final
    {
        return ModelType::NL;
    }

    void onClosedPluginWindow()
    {
        //dont change this
        _pWnd = nullptr;
    }
};

static Plugin s_plugin;

void onClosedPluginWindow()
{
    s_plugin.onClosedPluginWindow();
}

//Plugin requires extern C
extern "C"
{

PLUGIN_API sc::IPlugin* getPluginInterface()
{
    return &s_plugin;
}

}

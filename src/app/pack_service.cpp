/// @file
/// Implements `src/app/pack_service.h`; the interface is documented there.
#include "app/pack_service.h"

#include <algorithm>

#include "core/files.h"
#include "core/log.h"
#include "core/selector.h"
#include "core/strings.h"

namespace xn {

PackService::~PackService() {
  // A joinable thread must not be destroyed, whatever else happened.
  if (scanThread_.joinable()) {
    scanCancel_ = true;
    scanThread_.join();
  }
}

void PackService::startScan(const std::string& extraPaths) {
  stopScan();
  std::vector<std::string> roots = {packsDir_};
  for (const std::string& extra : splitList(extraPaths, ";"))
    roots.push_back(extra);
  scanCancel_ = false;
  scanDone_ = false;
  scanning_ = true;
  scannedPaths_ = extraPaths;
  logInfo("scanning Airline Packs in %s", join(roots, "; ").c_str());
  scanThread_ = std::thread([this, roots] {
    // The worker touches the filesystem only.
    std::shared_ptr<PackLibrary> lib;
    try {
      lib = std::make_shared<PackLibrary>(scanPacks(roots, &scanCancel_));
    } catch (const std::exception& e) {
      lib = std::make_shared<PackLibrary>();
      lib->warnings.push_back(std::string("scan failed: ") + e.what());
    } catch (...) {
      lib = std::make_shared<PackLibrary>();
      lib->warnings.push_back("scan failed");
    }
    std::lock_guard<std::mutex> lock(scanMu_);
    scanResult_ = std::move(lib);
    scanDone_ = true;
  });
}

void PackService::stopScan() {
  if (scanThread_.joinable()) {
    scanCancel_ = true;
    scanThread_.join();
  }
  scanning_ = false;
}

bool PackService::poll(const std::function<void()>& beforeSwap) {
  if (!scanning_ || !scanDone_) return false;
  if (scanThread_.joinable()) scanThread_.join();
  scanning_ = false;
  std::shared_ptr<PackLibrary> lib;
  {
    std::lock_guard<std::mutex> lock(scanMu_);
    lib = std::move(scanResult_);
  }
  if (!lib) return false;
  // The built-in airline table, with the user's AirlinePacks/airlines.tsv in
  // front of it.
  airlines_ = AirlineTable::builtin();
  const std::string userTable = readTextFile(packsDir_ + "airlines.tsv");
  if (!userTable.empty()) airlines_.prepend(AirlineTable::parse(userTable));
  for (const std::string& w : lib->warnings) logWarn("%s", w.c_str());
  for (const Pack& p : lib->packs) {
    // Files that are not Fenix announcement types are counted, so the log
    // shows why they never play.
    const std::vector<std::string> custom = p.customTypes();
    int notPlayed = 0;
    for (const SoundFile& f : p.files) notPlayed += f.type ? 0 : 1;
    const std::string extra =
        custom.empty()
            ? std::string()
            : ", " + std::to_string(notPlayed) +
                  " not played (not Fenix types: " + join(custom, ", ") + ")";
    logInfo("pack %s: %d sounds%s%s%s", p.name.c_str(),
            static_cast<int>(p.files.size()), extra.c_str(),
            p.wrapper.empty() ? "" : (" (in " + p.wrapper + ")").c_str(),
            p.mappingOnly ? " (mapping only)" : "");
    for (const std::string& w : p.warnings)
      logDebug("pack %s: %s", p.name.c_str(), w.c_str());
  }
  logSummary("%d Airline Packs, %d sounds", static_cast<int>(lib->packs.size()),
             static_cast<int>(lib->soundCount()));
  // The active pack points into the old library. Remember its name, let the
  // caller stop what plays from it, then drop the pointer with the library.
  packBeforeScan_ = resolution_.pack ? resolution_.pack->name : std::string();
  if (beforeSwap) beforeSwap();
  resolution_.pack = nullptr;
  library_ = std::move(lib);
  return true;
}

void PackService::loadLiveryConfig(const std::string& liveryPath,
                                   const std::string& liveryName) {
  const Ini sidecar =
      liveryPath.empty()
          ? Ini{}
          : Ini::parse(readTextFile(liveryPath + "airlinepack.cfg"));
  const Ini central = Ini::parse(readTextFile(packsDir_ + "liveries.ini"));
  liveryCfg_ = mergeLiveryConfig(liveryConfigFromSidecar(sidecar),
                                 liveryConfigFromCentral(central, liveryName));
  if (!liveryCfg_.source.empty())
    logInfo("livery config from %s", liveryCfg_.source.c_str());
}

std::vector<std::string> PackService::customTags(
    bool satcom, const std::string& satcomTags) const {
  std::vector<std::string> tags = liveryCfg_.customTags;
  if (satcom)
    for (const std::string& t : splitList(satcomTags))
      if (std::none_of(tags.begin(), tags.end(),
                       [&](const std::string& x) { return iequals(x, t); }))
        tags.push_back(t);
  return tags;
}

void PackService::resolve(const Request& request) {
  // Taken before the lock check, so a change while the pack is locked is
  // noticed once and not on every tick.
  resolvedFlightNumber_ = request.flightNumber;
  resolvedCallsign_ = request.ofpCallsign;
  if (!library_) return;
  if (request.packLocked && resolution_.pack) {
    logDebug("pack locked until the next flight; not re-resolving (%s)",
             request.why);
    return;
  }
  const Resolution r =
      resolvePack(*library_, airlines_,
                  ResolveInput{request.liveryName, request.flightNumber,
                               liveryCfg_, request.ofpCallsign});
  const bool changed = r.pack != resolution_.pack;
  resolution_ = r;
  if (changed || std::string(request.why) == "activated")
    logSummary("airline pack: %s (%s)", r.pack ? r.pack->name.c_str() : "none",
               r.reason.c_str());
  const std::string name = r.pack ? r.pack->name : std::string();
  const bool packChanged = name != activePackName_;
  activePackName_ = name;
  updateVariant(packChanged);
}

bool PackService::keepPack(const std::string& name) {
  if (!library_ || name.empty()) return false;
  const Pack* same = library_->find(name);
  if (!same) return false;
  resolution_.pack = same;
  updateVariant(false);
  return true;
}

void PackService::clear() {
  resolution_ = Resolution{};
  activePackName_.clear();
}

void PackService::dropLibrary() {
  resolution_.pack = nullptr;
  library_.reset();
}

void PackService::updateVariant(bool reroll) {
  const Pack* pack = resolution_.pack;
  const std::vector<int> choices =
      pack ? pack->variantChoices() : std::vector<int>{0};
  if (!reroll &&
      std::find(choices.begin(), choices.end(), variant_) != choices.end())
    return;
  variant_ = pack ? pickVariant(*pack, rng_) : 0;
  if (pack && (choices.size() > 1 || variant_ != 0)) {
    std::string list;
    for (int v : choices)
      list += (list.empty() ? "" : ", ") + std::to_string(v);
    logInfo("pack %s: variant %d (the pack has %s)", pack->name.c_str(),
            variant_, list.c_str());
  }
}

}  // namespace xn

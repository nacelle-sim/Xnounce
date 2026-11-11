/// @file
/// Everything about Airline Packs between the disk and the player: scanning
/// the pack folders in the background, the livery's pack configuration,
/// choosing the active pack and its variant (VP_SPEC § 1.4, § 3.4).
///
/// It needs no X-Plane: the scan worker only touches the filesystem, and the
/// plugin tells the service what it needs to know about the flight.
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "core/airlines.h"
#include "core/pack_library.h"
#include "core/pack_resolver.h"
#include "core/settings.h"

namespace xn {

class PackService {
 public:
  PackService() = default;
  /// Stops a running scan. Safe at any time, also from a static destructor.
  ~PackService();
  PackService(const PackService&) = delete;
  PackService& operator=(const PackService&) = delete;

  /// The folder `Resources/plugins/AirlinePacks/` with a trailing separator.
  /// It is always scanned and holds the optional `liveries.ini` and
  /// `airlines.tsv`.
  void setPacksDir(std::string dir) { packsDir_ = std::move(dir); }
  const std::string& packsDir() const { return packsDir_; }

  /// Starts a background scan of the packs folder and of `extraPaths` (the
  /// user's `pack_paths` setting, separated by ';'). A running scan is
  /// stopped first.
  void startScan(const std::string& extraPaths);
  /// Stops a running scan and waits for the worker.
  void stopScan();
  /// Picks up a finished scan: loads the airline table, logs the packs and
  /// installs the new library. `beforeSwap` runs just before the old library
  /// is dropped, so the caller can stop audio that plays from it. Returns
  /// true when a new library was installed; the active pack is then cleared
  /// and the caller either keeps it by name (`keepPack`) or resolves again.
  bool poll(const std::function<void()>& beforeSwap);
  bool scanning() const { return scanning_; }
  /// The extra paths of the last scan, to notice when the setting changes.
  const std::string& scannedPaths() const { return scannedPaths_; }
  /// Null until the first scan is done.
  const PackLibrary* library() const { return library_.get(); }
  const AirlineTable& airlines() const { return airlines_; }

  /// Reads the livery's `airlinepack.cfg` and the central `liveries.ini`
  /// (VP_SPEC § 1.4). `liveryPath` has a trailing separator and is empty for
  /// the default livery.
  void loadLiveryConfig(const std::string& liveryPath,
                        const std::string& liveryName);
  const LiveryConfig& liveryConfig() const { return liveryCfg_; }
  /// The custom tags of this flight: the livery's, plus `satcomTags` when a
  /// satellite antenna is fitted (VP_SPEC § 3.1, D-5).
  std::vector<std::string> customTags(bool satcom,
                                      const std::string& satcomTags) const;

  /// What decides the active pack.
  struct Request {
    std::string liveryName;
    std::string flightNumber;
    /// Callsign of the SimBrief plan while the plan is in use, else empty.
    std::string ofpCallsign;
    /// The purser has locked the pack for this flight (from ArmDoors on).
    bool packLocked = false;
    /// Why now, for the log. "activated" always logs the result.
    const char* why = "";
  };
  /// Chooses the active pack, unless it is locked, and re-rolls the variant
  /// when the pack changed.
  void resolve(const Request& request);
  /// After a new library: makes the pack called `name` active again when it
  /// still exists and keeps the variant if the pack still has it. Returns
  /// false when the pack is gone.
  bool keepPack(const std::string& name);
  /// Name of the pack that was active when the last scan finished, or empty.
  const std::string& packBeforeScan() const { return packBeforeScan_; }
  /// Forgets the active pack (the plugin went dormant).
  void clear();
  /// Forgets the scanned packs as well (the plugin is being disabled).
  void dropLibrary();

  const Resolution& resolution() const { return resolution_; }
  const Pack* pack() const { return resolution_.pack; }
  /// The session variant in use (VP_SPEC § 3.4, D-27).
  int variant() const { return variant_; }
  /// The flight number and callsign the last `resolve` saw, so the caller
  /// can tell when either changes.
  const std::string& resolvedFlightNumber() const {
    return resolvedFlightNumber_;
  }
  const std::string& resolvedCallsign() const { return resolvedCallsign_; }

 private:
  void updateVariant(bool reroll);

  std::string packsDir_;
  std::string scannedPaths_;

  /// Replaced as a whole when a scan finishes. Main thread only.
  std::shared_ptr<const PackLibrary> library_;
  AirlineTable airlines_;
  LiveryConfig liveryCfg_;
  Resolution resolution_;
  std::string resolvedFlightNumber_;
  std::string resolvedCallsign_;
  std::string activePackName_;
  std::string packBeforeScan_;
  int variant_ = 0;
  std::mt19937 rng_{std::random_device{}()};

  std::thread scanThread_;
  std::atomic<bool> scanCancel_{false};
  std::atomic<bool> scanDone_{false};
  std::mutex scanMu_;
  std::shared_ptr<PackLibrary> scanResult_;
  bool scanning_ = false;
};

}  // namespace xn

#include "PrometheusManager.h"

#include <stdexcept>

using namespace prometheus;

PrometheusManager::PrometheusManager()
    : _registry(std::make_shared<Registry>()),
      // Initialization
      _runNumberGauge(BuildGauge().Name("run_number").Help("Run Number").Register(*_registry).Add({})),
      _rateGauge(BuildGauge().Name("event_rate").Help("Acquisition rate").Register(*_registry).Add({})),
      _eventsGauge(BuildGauge().Name("events_total").Help("Events acquired").Register(*_registry).Add({})),
      _desyncEventsGauge(BuildGauge()
                             .Name("desync_events_total")
                             .Help("Events with desyncronized FEMs")
                             .Register(*_registry)
                             .Add({})),
      _infoFamily(BuildGauge().Name("run_metadata_info").Help("Run Metadata").Register(*_registry)),
      _runNameFamily(BuildGauge().Name("run_filename_info").Help("Run Filename").Register(*_registry)) {}

void PrometheusManager::StartServer(const std::string& address) {
    try {
        _exposer = std::make_unique<Exposer>(address);
        _exposer->RegisterCollectable(_registry);
    } catch (const std::exception& e) {
        _exposer = nullptr;
        throw std::runtime_error("Prometheus Error: " + std::string(e.what()));
    }
}

void PrometheusManager::setRunNumber(int num) { _runNumberGauge.Set(num); }

void PrometheusManager::setRate(double rate) { _rateGauge.Set(rate); }

void PrometheusManager::setEvents(int count) { _eventsGauge.Set(count); }

void PrometheusManager::setDesyncEvents(int count) { _desyncEventsGauge.Set(count); }

void PrometheusManager::setMetadata(const std::string& metadata) {
    if (_infoGauge) _infoFamily.Remove(_infoGauge);
    _infoGauge = &_infoFamily.Add({{"metadata", metadata}});
    _infoGauge->Set(1);
}

void PrometheusManager::setRunName(const std::string& name) {
    if (_runNameGauge) _runNameFamily.Remove(_runNameGauge);
    _runNameGauge = &_runNameFamily.Add({{"filename", name}});
    _runNameGauge->Set(1);
}

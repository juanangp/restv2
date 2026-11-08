
#pragma once

#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include <memory>
#include <string>

class PrometheusManager {
   public:
    // Constructor
    PrometheusManager();

    void setRunNumber(int num);
    void setRate(double rate);
    void setEvents(int count);
    void setDesyncEvents(int count);
    void setMetadata(const std::string& name);
    void setRunName(const std::string& name);

    void StartServer(const std::string& address = "0.0.0.0:8080");

   private:
    std::unique_ptr<prometheus::Exposer> _exposer;
    std::shared_ptr<prometheus::Registry> _registry;

    prometheus::Gauge& _runNumberGauge;
    prometheus::Gauge& _rateGauge;
    prometheus::Gauge& _eventsGauge;
    prometheus::Gauge& _desyncEventsGauge;

    // Information as gauges <name>_info = 1 with labels (the series of a prometheus::Info, which is not in
    // prometheus-cpp 1.0, e.g. the Debian package)
    prometheus::Family<prometheus::Gauge>& _infoFamily;
    prometheus::Family<prometheus::Gauge>& _runNameFamily;
    // The current series of each one, removed when replaced (only the current run is exported)
    prometheus::Gauge* _infoGauge = nullptr;
    prometheus::Gauge* _runNameGauge = nullptr;
};

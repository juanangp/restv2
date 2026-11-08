
#pragma once

#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include <memory>
#include <string>

/// \file PrometheusManager.h
/// \brief Prometheus metrics of restDAQ.

/// \class PrometheusManager
/// \brief Prometheus metrics of the data taking: gauges run_number, event_rate, events_total and
/// desync_events_total, and the information series run_metadata_info and run_filename_info (value 1, the
/// information in a label).
class PrometheusManager {
   public:
    // Constructor
    /// \brief Registers the metrics; the server is not started (StartServer)
    PrometheusManager();

    /// \brief Gauge run_number
    void setRunNumber(int num);
    /// \brief Gauge event_rate (Hz)
    void setRate(double rate);
    /// \brief Gauge events_total: events stored in the run
    void setEvents(int count);
    /// \brief Gauge desync_events_total: events of the run with the FEMs out of sync
    void setDesyncEvents(int count);
    /// \brief run_metadata_info with this text (the configuration dump) as the label metadata; replaces
    /// the previous series
    void setMetadata(const std::string& metadata);
    /// \brief run_filename_info with the output file name as the label filename; replaces the previous
    /// series
    void setRunName(const std::string& name);

    /// \brief Starts the HTTP server of the metrics at this address (host:port); throws on an error
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

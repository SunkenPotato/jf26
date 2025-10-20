#include <csignal>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <chrono>

#include <pigpio.h>
#include "../../include/httplib.h"

using namespace std;

#define GPIO_PIN 17

volatile sig_atomic_t keep_running = 1;

std::chrono::time_point<std::chrono::high_resolution_clock> program_start;

class Intervall2Bin
{
public:
    Intervall2Bin(int batch_laenge, int max_quantile)
        : batch_laenge(batch_laenge),
          max_quantile(max_quantile),
          vergleichsdaten_laenge(max_quantile * max_quantile)
    {
        quantile.reserve(max_quantile);
        vergleichsdaten.reserve(vergleichsdaten_laenge);
        NOT_READY.clear();
    }
    std::vector<unsigned int> take_intervall(unsigned int intervall);
    void fill_buffer(unsigned char *buf, int n);
    int batch_laenge = 1000; // Menge an Intervallen, die aufgenommen werden, bevor ein Signifikanztest ausgeführt wird
    int max_quantile;        // Maximale Anzahl an Quantilen, in die die Exp. Funktion eingeteilt wird
    std::vector<unsigned int> aktuelle_bins;

private:
    void bins_erstellen();
    unsigned int welcher_bin(double intervall);
    bool t_test();
    int referenz_zähler_vergleichsdaten = 0;   // Iterator für Länge der Vergleichsdaten
    std::vector<unsigned int> vergleichsdaten; // Daten, um erwartete akute Zerfallsrate zu bestimmen
    // Wird zurückgegeben, wenn noch das Programm noch nicht bereit ist (zB wenn die Vergleichsdaten nicht groß genug sind)
    std::vector<unsigned int> NOT_READY;
    int vergleichsdaten_laenge;
    std::vector<unsigned int> quantile;
    std::vector<unsigned int> intervalle_post_vergleichsverteilung;
    int post_vergleichsdaten_zähler = 0;
};

// Nimmt Intervalle in Mikrosekunden, sammelt zunächst Vergleichsdaten,
// lässt die Exponentialverteilung in gleichwahrscheinliche
// Quantile einteilen und lässt prüfen, in welchem Quantil, also Bin, sich das Intervall, mit dem
// diese Methode als letztes aufgerufen wurde, befindet und speichert diesem wert in bit_liste
std::vector<unsigned int> Intervall2Bin::take_intervall(unsigned int intervall)
{
    std::cout << "take_intervall called" << std::endl;
    // Überprüfen, ob genug Vergleichsdaten vorhanden
    if (referenz_zähler_vergleichsdaten < vergleichsdaten_laenge)
    {
        // Vergleichsdaten das neue Intervall hinzufügen
        vergleichsdaten.push_back(intervall);
        referenz_zähler_vergleichsdaten++;
        return NOT_READY;
    }
    else
    {
        // Wenn Exponentialverteilung noch nicht in Quantile eingeteilt wurde, einteilen
        if (quantile.empty())
            bins_erstellen();

        intervalle_post_vergleichsverteilung.push_back(intervall);

        // Lässt prüfen, in welchem Quantil sich das neue Intervall befindet
        aktuelle_bins.push_back(welcher_bin(intervall));

        post_vergleichsdaten_zähler++;

        if (post_vergleichsdaten_zähler % batch_laenge == 0)
        {
            // Ausführung eines Signifikanztests
            if (t_test())
            {
                // Wenn Vergleichsverteilung zu den neuen Intervallen signifikant unterschiedlich
                referenz_zähler_vergleichsdaten = 0;
                quantile.clear();
                intervalle_post_vergleichsverteilung.clear();
                vergleichsdaten.clear();
                aktuelle_bins.clear();
                return NOT_READY;
            }
            else
            {
                return aktuelle_bins;
            }
        }
        return NOT_READY;
    }
}

// Nimmt die Vergleichsdaten, schätzt damit das Lamda, also die Zerfallsrate der Dichtefunktion
// der Exponentialfunktion, und teilt diese in "max_quantile"
// quantile ein, die alle das Integral 1 / max_quantile haben und speichert diese in dem Vektor "quantiles"
void Intervall2Bin::bins_erstellen()
{
    // Lambda aus Vergleichsdaten schätzen
    double mean = std::accumulate(vergleichsdaten.begin(), vergleichsdaten.end(), 0.0) / vergleichsdaten.size();
    double lambda_hat = 1.0 / mean;

    // Quantile für gleichwahrscheinliche Quantile
    quantile.resize(max_quantile - 1);
    for (int k = 1; k < max_quantile; ++k)
    {
        double p = static_cast<double>(k) / max_quantile;
        quantile[k - 1] = -std::log(1.0 - p) / lambda_hat;
    }
}

// Prüft, in welchem Quantil sich ein Intervall befindet
unsigned int Intervall2Bin::welcher_bin(double intervall)
{
    unsigned int index;

    // Wenn das Intervall größer als die untere Grenze vom letzten Quantil ist, wird in index die Anzahl der Quantile - 1 gespeichert
    if (intervall > quantile.back())
    {
        index = max_quantile - 1;
    }
    else
    {
        // Sucht den ersten Wert in quantile, der größer ist als intervall.
        // it zeigt auf die Einfügeposition.
        auto it = std::upper_bound(quantile.begin(), quantile.end(), intervall);

        // Berechnet den Abstand zwischen dem ersten Wert in Quantile und it
        index = static_cast<int>(std::distance(quantile.begin(), it));
    }

    return index;
}

bool Intervall2Bin::t_test()
{
    // Berechnet die Mittelwerte der beiden Datensätze, also vergleichsdaten und intervalle_post_vergleichsverteilung
    double mean_base = std::accumulate(vergleichsdaten.begin(), vergleichsdaten.end(), 0.0) / vergleichsdaten.size();
    double mean_interv = std::accumulate(intervalle_post_vergleichsverteilung.begin(), intervalle_post_vergleichsverteilung.end(), 0.0) / intervalle_post_vergleichsverteilung.size();

    // Berechnet die Varianz der Vergleichsdaten
    double var_base = 0.0;
    for (double x : vergleichsdaten)
        var_base += (x - mean_base) * (x - mean_base);
    var_base /= (vergleichsdaten.size() - 1);

    // Berechnet die Varianz der intervalle_post_vergleichsverteilung
    double var_interv = 0.0;
    for (double x : intervalle_post_vergleichsverteilung)
        var_interv += (x - mean_interv) * (x - mean_interv);
    var_interv /= (intervalle_post_vergleichsverteilung.size() - 1);

    // Berechnet den t-Wert
    double t = std::abs(mean_base - mean_interv) / std::sqrt(var_base / vergleichsdaten.size() + var_interv / intervalle_post_vergleichsverteilung.size());

    // Gibt True zurück, wenn signifikant unterschiedlich, sonst false
    const double t_crit = 2.58; // ungefähr 99% Konfidenz
    return t > t_crit;
}

void Intervall2Bin::fill_buffer(unsigned char *buf, int n)
{
    // TODO(SunkenPotato): actually fill this with random bytes.
    for (int i = 0; i < n; i += 1)
    {
        buf[i] = aktuelle_bins.back();
        aktuelle_bins.pop_back();
    }
}

std::mutex converter_mutex;
Intervall2Bin converter;

void initRoutes(httplib::Server &svr)
{
    svr.Get("/", [](const httplib::Request &req, httplib::Response &res)
            {
        size_t n = 32;

        if (req.has_param("amount")) {
            try {
                n = std::stoi(req.get_param_value("amount"));
                if (n <= 0 || n >= 4096) n = 32;
            } catch (...) {
                n = 32;
            }
        } else {
            res.status = 400;
            res.set_content("Missing parameter 'amount'", "text/plain");
            return;
        }

        unsigned char buffer[4096];
        std::lock_guard<std::mutex> lock_guard(converter_mutex);
        if (converter.aktuelle_bins.size() < n) {
            res.status = 503;
            res.set_content("That amount of bytes is not available", "text/plain");
            return;
        }

        converter.fill_buffer(buffer, n);

        std::string_view body(reinterpret_cast<const char*>(buffer), n);
        res.set_content_provider(n, "application/octet-stream", [buffer](size_t offset, size_t len, httplib::DataSink &sink) {
            sink.write(reinterpret_cast<const char*>(buffer + offset), len);
            return true;
        });
        res.status = 200;

        return; });
}

std::optional<std::chrono::time_point<std::chrono::high_resolution_clock>> last_particle;

void gpioHook(int gpio, int level, unsigned int tick)
{
    if (level == 0)
    {
        std::cout << "got a particle" << std::endl;
        if (!last_particle)
        {
            last_particle = std::chrono::high_resolution_clock::now();
            return;
        }
        else
        {
            std::chrono::time_point now = std::chrono::high_resolution_clock::now();
            unsigned int interval = std::chrono::duration_cast<std::chrono::microseconds>(now - *last_particle).count();
            last_particle = now;
            std::lock_guard guard(converter_mutex);
            converter.take_intervall(interval);
        }
    }
}

int initGpio()
{
    if (gpioInitialise() < 0)
    {
        std::cout << "Failed to call gpioInitialise" << std::endl;
        return 1;
    };
    if (gpioSetMode(GPIO_PIN, PI_INPUT) != 0)
    {
        std::cout << "Failed to set input mode" << std::endl;
        return 2;
    }
    if (gpioSetAlertFunc(GPIO_PIN, gpioHook) != 0)
    {
        std::cout << "Failed to set alert function" << std::endl;
        return 3;
    }

    return 0;
}

int main()
{
    program_start = std::chrono::high_resolution_clock::now();
    if (initGpio() != 0)
    {
        std::cout << "Failed to initialise GPIO" << std::endl;
        gpioTerminate();
        return 1;
    }
    httplib::Server svr;
    initRoutes(svr);

    svr.listen("127.0.0.1", 8000);

    gpioTerminate();
    return 0;
}

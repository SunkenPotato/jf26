#include <csignal>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>

//#include <pigpio.h>
#include "../../include/httplib.h"

using namespace std;

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
        if (quantile.empty()){
            zeitstempel("Neue Batch");
            bins_erstellen();
            std::vector<double> quantile_d(quantile.begin(), quantile.end());
            zeitstempel("Quantile", quantile_d);
        }
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
    zeitstempel("Zerfallsrate", {lambda_hat});

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
    zeitstempel("t-Wert", {t});

    // Gibt True zurück, wenn signifikant unterschiedlich, sonst false
    const double t_crit = 1.96; // ungefähr 95% Konfidenz
    return t > t_crit;
}

void zeitstempel(const std::string& typ, const std::vector<double>& daten = {}) {
    std::ofstream params("../../qualitätstest/ergebnisparams.csv", std::ios::app);
    if (!params.is_open()) return;

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    params << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "," << typ;

    for (const auto& val : daten) {
        params << "," << val;
    }
    params << "\n";
}


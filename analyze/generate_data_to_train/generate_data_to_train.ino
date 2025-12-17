/*
  ============================================================================
  CAPTURE DE DONNÉES IMU POUR L'ENTRAÎNEMENT
  ============================================================================
  
  Ce programme capture les données de l'accéléromètre et du gyroscope
  de l'Arduino Nano 33 BLE Sense et les envoie sur le port série au format CSV.
  
  FONCTIONNEMENT :
  1. Attend qu'un mouvement significatif soit détecté (seuil d'accélération)
  2. Capture 119 échantillons de données (accélération + gyroscope)
  3. Envoie les données au format CSV sur le port série
  4. Répète le processus pour capturer plusieurs gestes
  
  UTILISATION :
  - Connectez l'Arduino au PC via USB
  - Ouvrez le moniteur série à 9600 bauds
  - Effectuez un geste (punch ou flex) quand le programme attend
  - Les données seront envoyées au format CSV
  - Copiez-collez les données dans un fichier CSV (punch.csv ou flex.csv)
  
  MATÉRIEL REQUIS :
  - Arduino Nano 33 BLE ou Arduino Nano 33 BLE Sense
  
  Créé par Don Coleman, Sandeep Mistry
  Modifié par Dominic Pajak, Sandeep Mistry
  Code dans le domaine public.
*/

// ============================================================================
// INCLUSIONS
// ============================================================================
#include <Arduino_LSM9DS1.h>  // Bibliothèque pour l'IMU (accéléromètre + gyroscope)

// ============================================================================
// CONSTANTES DE CONFIGURATION
// ============================================================================

// Seuil d'accélération pour détecter un mouvement significatif (en G)
// Si la somme des valeurs absolues des accélérations dépasse ce seuil,
// on considère qu'un geste commence
// 2.5 G = mouvement assez fort pour être un geste intentionnel
const float accelerationThreshold = 2.5;

// Nombre d'échantillons à capturer par geste
// 119 échantillons = environ 2 secondes à 50 Hz (fréquence d'échantillonnage)
// Cette valeur DOIT correspondre à SAMPLES_PER_GESTURE dans le notebook Python
const int numSamples = 119;

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

// Compteur d'échantillons lus depuis le dernier mouvement détecté
// Initialisé à numSamples pour indiquer qu'aucun geste n'est en cours
// Quand un mouvement est détecté, on le remet à 0
int samplesRead = numSamples;

// ============================================================================
// FONCTION SETUP - EXÉCUTÉE UNE SEULE FOIS AU DÉMARRAGE
// ============================================================================
void setup() {
  // Initialiser la communication série à 9600 bauds
  // Cette vitesse doit correspondre à celle du moniteur série
  Serial.begin(9600);
  
  // Attendre que le port série soit ouvert (important pour certains systèmes)
  // Sans cette ligne, les premiers messages peuvent être perdus
  while (!Serial);

  // Initialiser l'IMU (Inertial Measurement Unit)
  // L'IMU contient l'accéléromètre et le gyroscope
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    // Si l'initialisation échoue, arrêter le programme
    while (1);
  }

  // Afficher l'en-tête CSV
  // Cette ligne sera la première ligne du fichier CSV
  // Format : aX (accélération X), aY, aZ, gX (gyroscope X), gY, gZ
  Serial.println("aX,aY,aZ,gX,gY,gZ");
}

// ============================================================================
// FONCTION LOOP - EXÉCUTÉE EN BOUCLE CONTINUE
// ============================================================================
void loop() {
  // Variables pour stocker les valeurs de l'IMU
  float aX, aY, aZ;  // Accélération sur les axes X, Y, Z (en G)
  float gX, gY, gZ;  // Vitesse angulaire (gyroscope) sur les axes X, Y, Z (en deg/s)

  // ========================================================================
  // PHASE 1 : ATTENDRE UN MOUVEMENT SIGNIFICATIF
  // ========================================================================
  // Cette boucle attend qu'un mouvement soit détecté
  // Elle s'exécute tant que samplesRead == numSamples (aucun geste en cours)
  while (samplesRead == numSamples) {
    // Vérifier si de nouvelles données d'accélération sont disponibles
    if (IMU.accelerationAvailable()) {
      // Lire les valeurs d'accélération
      // Les valeurs sont en G (gravité terrestre)
      // -4G à +4G est la plage typique pour l'Arduino Nano 33 BLE Sense
      IMU.readAcceleration(aX, aY, aZ);

      // Calculer la somme des valeurs absolues des accélérations
      // Cela donne une mesure de l'intensité globale du mouvement
      // fabs() = fonction valeur absolue pour les float
      float aSum = fabs(aX) + fabs(aY) + fabs(aZ);

      // Vérifier si le mouvement est assez fort pour être considéré comme un geste
      if (aSum >= accelerationThreshold) {
        // Un mouvement significatif a été détecté !
        // Réinitialiser le compteur pour commencer la capture
        samplesRead = 0;
        break;  // Sortir de la boucle d'attente
      }
    }
  }

  // ========================================================================
  // PHASE 2 : CAPTURER LES DONNÉES DU GESTE
  // ========================================================================
  // Cette boucle capture numSamples échantillons depuis la détection du mouvement
  // Elle s'exécute tant que samplesRead < numSamples
  while (samplesRead < numSamples) {
    // Vérifier si de nouvelles données sont disponibles pour les deux capteurs
    // Il est important d'attendre que les deux soient disponibles pour
    // avoir des données synchronisées
    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
      // Lire les données de l'accéléromètre
      // Valeurs en G (gravité terrestre)
      IMU.readAcceleration(aX, aY, aZ);
      
      // Lire les données du gyroscope
      // Valeurs en degrés par seconde (deg/s)
      // Plage typique : -2000 à +2000 deg/s
      IMU.readGyroscope(gX, gY, gZ);

      // Incrémenter le compteur d'échantillons
      samplesRead++;

      // ====================================================================
      // ENVOYER LES DONNÉES AU FORMAT CSV
      // ====================================================================
      // Format CSV : aX,aY,aZ,gX,gY,gZ
      // Les valeurs sont affichées avec 3 décimales pour la précision
      
      // Accélération X
      Serial.print(aX, 3);
      Serial.print(',');
      
      // Accélération Y
      Serial.print(aY, 3);
      Serial.print(',');
      
      // Accélération Z
      Serial.print(aZ, 3);
      Serial.print(',');
      
      // Gyroscope X
      Serial.print(gX, 3);
      Serial.print(',');
      
      // Gyroscope Y
      Serial.print(gY, 3);
      Serial.print(',');
      
      // Gyroscope Z
      Serial.print(gZ, 3);
      Serial.println();  // Nouvelle ligne après chaque échantillon

      // Si on a capturé tous les échantillons nécessaires
      if (samplesRead == numSamples) {
        // Ajouter une ligne vide pour séparer les gestes dans le CSV
        // Cela facilite la lecture et le traitement des données
        Serial.println();
        
        // Après cette ligne, samplesRead == numSamples, donc on retourne
        // à la phase 1 pour attendre le prochain geste
      }
    }
  }
}

/*
  ============================================================================
  NOTES IMPORTANTES
  ============================================================================
  
  1. SYNCHRONISATION :
     - Le programme attend que les deux capteurs (accéléromètre et gyroscope)
       aient des données disponibles avant de les lire
     - Cela garantit que les données sont synchronisées
  
  2. FORMAT DES DONNÉES :
     - Accélération : en G (gravité terrestre), plage typique -4G à +4G
     - Gyroscope : en deg/s, plage typique -2000 à +2000 deg/s
     - Ces plages sont utilisées pour la normalisation dans le notebook Python
  
  3. CAPTURE DE PLUSIEURS GESTES :
     - Pour capturer plusieurs gestes, effectuez simplement un nouveau geste
       après que le précédent soit terminé
     - Chaque geste = 119 lignes de données dans le CSV
     - Copiez-collez les données dans un fichier CSV séparé pour chaque geste
  
  4. UTILISATION AVEC PYTHON :
     - Les données capturées doivent être sauvegardées dans des fichiers CSV
     - Nommez-les selon vos gestes : punch.csv, flex.csv, etc.
     - Ces fichiers seront utilisés dans le notebook Jupyter pour l'entraînement
*/

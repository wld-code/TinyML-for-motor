/*
  ============================================================================
  CLASSIFICATION DE GESTES AVEC TENSORFLOW LITE
  ============================================================================
  
  Ce programme utilise l'IMU intégré de l'Arduino Nano 33 BLE Sense pour :
  1. Détecter un mouvement significatif (seuil d'accélération)
  2. Capturer 119 échantillons de données (accélération + gyroscope)
  3. Normaliser les données (comme pendant l'entraînement)
  4. Utiliser un modèle TensorFlow Lite pour classifier le geste
  5. Afficher les probabilités pour chaque geste
  
  FONCTIONNEMENT :
  - Le modèle a été entraîné dans le notebook Jupyter
  - Il a été converti en TensorFlow Lite et intégré dans model.h
  - Le programme utilise ce modèle pour prédire quel geste a été effectué
  
  MATÉRIEL REQUIS :
  - Arduino Nano 33 BLE ou Arduino Nano 33 BLE Sense
  - Bibliothèque TensorFlow Lite for Microcontrollers installée
  
  Créé par Don Coleman, Sandeep Mistry
  Modifié par Dominic Pajak, Sandeep Mistry
  Code dans le domaine public.
*/

// ============================================================================
// INCLUSIONS
// ============================================================================

// Bibliothèque pour l'IMU (accéléromètre + gyroscope)
#include <Arduino_LSM9DS1.h>

// Bibliothèques TensorFlow Lite for Microcontrollers
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>  // Résolveur pour toutes les opérations
#include <tensorflow/lite/micro/micro_error_reporter.h>  // Gestionnaire d'erreurs
#include <tensorflow/lite/micro/micro_interpreter.h>  // Interpréteur pour exécuter le modèle
#include <tensorflow/lite/schema/schema_generated.h>  // Schéma du modèle
#include <tensorflow/lite/version.h>  // Version de TensorFlow Lite

// Fichier header contenant le modèle entraîné
// Ce fichier a été généré par le notebook Jupyter
#include "model.h"

// ============================================================================
// CONSTANTES DE CONFIGURATION
// ============================================================================

// Seuil d'accélération pour détecter un mouvement significatif (en G)
// Si la somme des valeurs absolues des accélérations dépasse ce seuil,
// on considère qu'un geste commence
// 2.5 G = mouvement assez fort pour être un geste intentionnel
const float accelerationThreshold = 2.5;

// Nombre d'échantillons à capturer par geste
// 119 échantillons = environ 2 secondes à 50 Hz
// Cette valeur DOIT correspondre à SAMPLES_PER_GESTURE dans le notebook Python
const int numSamples = 119;

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

// Compteur d'échantillons lus depuis le dernier mouvement détecté
// Initialisé à numSamples pour indiquer qu'aucun geste n'est en cours
int samplesRead = numSamples;

// ============================================================================
// VARIABLES TENSORFLOW LITE
// ============================================================================

// Gestionnaire d'erreurs pour TensorFlow Lite
// Utilisé pour rapporter les erreurs pendant l'exécution du modèle
tflite::MicroErrorReporter tflErrorReporter;

// Résolveur d'opérations TensorFlow Lite
// Contient toutes les opérations disponibles (add, mul, relu, softmax, etc.)
// Note : On pourrait n'inclure que les opérations nécessaires pour réduire
//        la taille du code compilé
tflite::AllOpsResolver tflOpsResolver;

// Pointeur vers le modèle TensorFlow Lite
// Le modèle est chargé depuis model.h (tableau de bytes)
const tflite::Model* tflModel = nullptr;

// Interpréteur TensorFlow Lite
// C'est l'objet qui exécute le modèle sur les données d'entrée
tflite::MicroInterpreter* tflInterpreter = nullptr;

// Tenseur d'entrée du modèle
// Contient les données normalisées (119 échantillons × 6 valeurs = 714 valeurs)
TfLiteTensor* tflInputTensor = nullptr;

// Tenseur de sortie du modèle
// Contient les probabilités pour chaque geste (2 valeurs : [punch, flex])
TfLiteTensor* tflOutputTensor = nullptr;

// Tampon mémoire statique pour TensorFlow Lite
// Le modèle a besoin de mémoire pour stocker les valeurs intermédiaires
// 8 KB = 8192 bytes (peut nécessiter un ajustement selon le modèle)
constexpr int tensorArenaSize = 8 * 1024;

// Tableau de bytes aligné sur 16 bytes pour optimiser l'accès mémoire
// L'alignement est important pour les performances sur microcontrôleurs
byte tensorArena[tensorArenaSize] __attribute__((aligned(16)));

// ============================================================================
// CONFIGURATION DES GESTES
// ============================================================================

// Tableau des noms de gestes
// L'ordre DOIT correspondre à l'ordre utilisé pendant l'entraînement
// Index 0 = "punch" → correspond à [1, 0] en one-hot encoding
// Index 1 = "flex" → correspond à [0, 1] en one-hot encoding
const char* GESTURES[] = {
  "punch",  // Coup de poing
  "flex"    // Flexion
};

// Calculer le nombre de gestes automatiquement
// sizeof(GESTURES) = taille totale du tableau en bytes
// sizeof(GESTURES[0]) = taille d'un élément (pointeur) en bytes
// Le résultat est le nombre d'éléments dans le tableau
#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))

// ============================================================================
// FONCTION SETUP - EXÉCUTÉE UNE SEULE FOIS AU DÉMARRAGE
// ============================================================================
void setup() {
  // Initialiser la communication série à 9600 bauds
  Serial.begin(9600);
  
  // Attendre que le port série soit ouvert
  while (!Serial);

  // ========================================================================
  // INITIALISATION DE L'IMU
  // ========================================================================
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    // Si l'initialisation échoue, arrêter le programme
    while (1);
  }

  // Afficher les fréquences d'échantillonnage de l'IMU
  // Ces informations sont utiles pour comprendre la fréquence de capture
  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");
  
  Serial.print("Gyroscope sample rate = ");
  Serial.print(IMU.gyroscopeSampleRate());
  Serial.println(" Hz");
  
  Serial.println();

  // ========================================================================
  // CHARGEMENT DU MODÈLE TENSORFLOW LITE
  // ========================================================================
  
  // Obtenir le modèle TensorFlow Lite depuis le tableau de bytes dans model.h
  // Le tableau 'model' est défini dans model.h (généré par le notebook)
  tflModel = tflite::GetModel(model);
  
  // Vérifier que la version du schéma correspond
  // Si la version ne correspond pas, le modèle pourrait ne pas fonctionner
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    Serial.print("Model version: ");
    Serial.println(tflModel->version());
    Serial.print("Required version: ");
    Serial.println(TFLITE_SCHEMA_VERSION);
    while (1);  // Arrêter le programme
  }

  // ========================================================================
  // CRÉATION DE L'INTERPRÉTEUR TENSORFLOW LITE
  // ========================================================================
  
  // Créer un interpréteur pour exécuter le modèle
  // Paramètres :
  //   - tflModel : le modèle à exécuter
  //   - tflOpsResolver : résolveur d'opérations (contient les fonctions mathématiques)
  //   - tensorArena : tampon mémoire pour les calculs intermédiaires
  //   - tensorArenaSize : taille du tampon mémoire
  //   - &tflErrorReporter : gestionnaire d'erreurs
  tflInterpreter = new tflite::MicroInterpreter(
    tflModel, 
    tflOpsResolver, 
    tensorArena, 
    tensorArenaSize, 
    &tflErrorReporter
  );

  // Allouer la mémoire pour les tenseurs d'entrée et de sortie
  // Cette étape prépare la mémoire nécessaire pour les calculs
  tflInterpreter->AllocateTensors();

  // ========================================================================
  // OBTENIR LES POINTEURS VERS LES TENSEURS
  // ========================================================================
  
  // Obtenir le pointeur vers le tenseur d'entrée (index 0)
  // C'est ici qu'on mettra les données normalisées
  tflInputTensor = tflInterpreter->input(0);
  
  // Obtenir le pointeur vers le tenseur de sortie (index 0)
  // C'est ici qu'on récupérera les probabilités de chaque geste
  tflOutputTensor = tflInterpreter->output(0);
  
  // Afficher des informations sur les tenseurs (optionnel, pour debug)
  Serial.println("Model loaded successfully!");
  Serial.print("Input tensor size: ");
  Serial.println(tflInputTensor->bytes);
  Serial.print("Output tensor size: ");
  Serial.println(tflOutputTensor->bytes);
  Serial.println();
}

// ============================================================================
// FONCTION LOOP - EXÉCUTÉE EN BOUCLE CONTINUE
// ============================================================================
void loop() {
  // Variables pour stocker les valeurs brutes de l'IMU
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
      IMU.readAcceleration(aX, aY, aZ);

      // Calculer la somme des valeurs absolues des accélérations
      // Cela donne une mesure de l'intensité globale du mouvement
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
  // PHASE 2 : CAPTURER ET TRAITER LES DONNÉES DU GESTE
  // ========================================================================
  // Cette boucle capture numSamples échantillons et les normalise
  while (samplesRead < numSamples) {
    // Vérifier si de nouvelles données sont disponibles pour les deux capteurs
    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
      // Lire les données brutes de l'IMU
      IMU.readAcceleration(aX, aY, aZ);
      IMU.readGyroscope(gX, gY, gZ);

      // ====================================================================
      // NORMALISATION DES DONNÉES
      // ====================================================================
      // IMPORTANT : Cette normalisation DOIT être identique à celle utilisée
      // pendant l'entraînement dans le notebook Python !
      //
      // Formule de normalisation : (valeur - min) / (max - min)
      // 
      // Pour l'accélération :
      //   - Plage : -4G à +4G
      //   - Normalisation : (valeur + 4) / 8
      //   - Résultat : 0.0 à 1.0
      //
      // Pour le gyroscope :
      //   - Plage : -2000 à +2000 deg/s
      //   - Normalisation : (valeur + 2000) / 4000
      //   - Résultat : 0.0 à 1.0
      //
      // Format d'entrée du modèle :
      //   - 119 échantillons × 6 valeurs = 714 valeurs au total
      //   - Ordre : [aX, aY, aZ, gX, gY, gZ] pour chaque échantillon
      
      // Calculer l'index dans le tableau d'entrée
      // samplesRead * 6 = position de départ pour cet échantillon
      // + 0, 1, 2, 3, 4, 5 = position de chaque valeur dans l'échantillon
      
      // Accélération X normalisée
      tflInputTensor->data.f[samplesRead * 6 + 0] = (aX + 4.0) / 8.0;
      
      // Accélération Y normalisée
      tflInputTensor->data.f[samplesRead * 6 + 1] = (aY + 4.0) / 8.0;
      
      // Accélération Z normalisée
      tflInputTensor->data.f[samplesRead * 6 + 2] = (aZ + 4.0) / 8.0;
      
      // Gyroscope X normalisé
      tflInputTensor->data.f[samplesRead * 6 + 3] = (gX + 2000.0) / 4000.0;
      
      // Gyroscope Y normalisé
      tflInputTensor->data.f[samplesRead * 6 + 4] = (gY + 2000.0) / 4000.0;
      
      // Gyroscope Z normalisé
      tflInputTensor->data.f[samplesRead * 6 + 5] = (gZ + 2000.0) / 4000.0;

      // Incrémenter le compteur d'échantillons
      samplesRead++;

      // ====================================================================
      // EXÉCUTER LE MODÈLE (INFÉRENCE)
      // ====================================================================
      // Si on a capturé tous les échantillons nécessaires
      if (samplesRead == numSamples) {
        // Exécuter le modèle TensorFlow Lite
        // Cette fonction prend les données d'entrée (tflInputTensor)
        // et calcule les probabilités de sortie (tflOutputTensor)
        TfLiteStatus invokeStatus = tflInterpreter->Invoke();
        
        // Vérifier si l'exécution a réussi
        if (invokeStatus != kTfLiteOk) {
          Serial.println("Invoke failed!");
          Serial.print("Status code: ");
          Serial.println(invokeStatus);
          while (1);  // Arrêter le programme en cas d'erreur
          return;
        }

        // ====================================================================
        // AFFICHER LES RÉSULTATS
        // ====================================================================
        // Le tenseur de sortie contient les probabilités pour chaque geste
        // Exemple : [0.95, 0.05] signifie 95% punch, 5% flex
        
        // Parcourir tous les gestes
        for (int i = 0; i < NUM_GESTURES; i++) {
          // Afficher le nom du geste
          Serial.print(GESTURES[i]);
          Serial.print(": ");
          
          // Afficher la probabilité avec 6 décimales
          // tflOutputTensor->data.f[i] contient la probabilité du geste i
          Serial.println(tflOutputTensor->data.f[i], 6);
        }
        
        // Ligne vide pour séparer les résultats
        Serial.println();
        
        // Après l'affichage, samplesRead == numSamples, donc on retourne
        // à la phase 1 pour attendre le prochain geste
      }
    }
  }
}

/*
  ============================================================================
  NOTES IMPORTANTES
  ============================================================================
  
  1. NORMALISATION :
     - La normalisation DOIT être identique à celle utilisée pendant l'entraînement
     - Si vous changez la normalisation dans le notebook, changez-la aussi ici
     - Les plages (-4 à +4 pour accélération, -2000 à +2000 pour gyroscope)
       doivent correspondre aux plages réelles de votre capteur
  
  2. FORMAT DES DONNÉES :
     - Le modèle attend 714 valeurs (119 échantillons × 6 valeurs)
     - Ordre : [aX, aY, aZ, gX, gY, gZ] répété 119 fois
     - Toutes les valeurs doivent être normalisées entre 0.0 et 1.0
  
  3. INTERPRÉTATION DES RÉSULTATS :
     - Les valeurs de sortie sont des probabilités (entre 0.0 et 1.0)
     - La somme des probabilités = 1.0 (grâce à softmax)
     - Le geste avec la probabilité la plus élevée est la prédiction
  
  4. PERFORMANCE :
     - L'exécution du modèle prend quelques millisecondes
     - La taille du modèle est limitée par la mémoire disponible
     - Le tampon tensorArena peut nécessiter un ajustement selon le modèle
  
  5. DÉBOGAGE :
     - Si le modèle ne fonctionne pas, vérifiez :
       * Que model.h est bien inclus
       * Que la normalisation est correcte
       * Que le nombre d'échantillons correspond
       * Que les versions de TensorFlow Lite sont compatibles
*/

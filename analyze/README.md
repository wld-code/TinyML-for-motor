# TinyML - Reconnaissance de Gestes sur Arduino

Guide rapide pour créer et déployer un modèle de reconnaissance de gestes sur Arduino Nano 33 BLE Sense.

## 📋 Prérequis

- Arduino Nano 33 BLE Sense
- Python 3.8-3.11 (TensorFlow ne supporte pas Python 3.12+)
- Arduino IDE
- VS Code

## 🚀 Étapes

### 1️⃣ Capturer les données d'entraînement

**Sur Arduino :**
1. Ouvrir `generate_data_to_train/generate_data_to_train.ino` dans Arduino IDE
2. Installer la bibliothèque `Arduino_LSM9DS1` (Gestionnaire de bibliothèques)
3. Sélectionner la carte : **Outils → Type de carte → Arduino Nano 33 BLE**
4. Téléverser le code
5. Ouvrir le Moniteur série (9600 bauds)

**Sur PC :**
1. Créer l'environnement virtuel : `python -m venv env` sur Git Bash 
2. Activer l'environnement virtuel :
   - Windows : `source env/Scripts/activate`
3. Installer pyserial : `pip install pyserial`
4. Modifier `SERIAL_PORT` dans `serial_data_to_csv.py` (ex: `'COM7'`)
5. Exécuter : `python serial_data_to_csv.py`
6. Effectuer des gestes (punch, flex) - le script capture automatiquement 119 échantillons par geste
7. Arrêter avec `Ctrl+C`, renommer `output.csv` en `punch.csv` ou `flex.csv`
8. Répéter pour chaque geste

### 2️⃣ Entraîner le modèle (Jupyter Notebook)

**Installer Jupyter dans VS Code :**
1. Ouvrir VS Code
2. Installer l'extension **Jupyter** (Microsoft)
3. Ouvrir `arduino_tiny_ml.ipynb`
4. Créer un environnement virtuel (recommandé) :
   ```bash
   python -m venv env
   ```
   - Windows : `.\env\Scripts\Activate.ps1` ou `env\Scripts\activate.bat`
   - Linux/Mac : `source env/bin/activate`
5. Installer les dépendances : `pip install numpy pandas matplotlib tensorflow`
6. Dans VS Code, sélectionner l'interpréteur Python de l'environnement virtuel (Ctrl+Shift+P → "Python: Select Interpreter" → choisir `env`)

**Exécuter le notebook :**
1. Exécuter les cellules dans l'ordre (Shift+Enter)
2. Le notebook génère `model.h` à la fin
3. Copier `model.h` dans le dossier `classify_imu/`

### 3️⃣ Déployer sur Arduino

1. Ouvrir `classify_imu/classify_imu.ino` dans Arduino IDE
2. Installer les bibliothèques :
   - `Arduino_LSM9DS1`
   - `TensorFlowLite` (via Gestionnaire de bibliothèques)
3. Vérifier que `model.h` est dans le même dossier que `classify_imu.ino`
4. Téléverser le code
5. Ouvrir le Moniteur série (9600 bauds)
6. Effectuer un geste → les probabilités s'affichent (ex: `punch: 0.95, flex: 0.05`)

## 📁 Structure des fichiers

```
analyze/
├── generate_data_to_train/    # Code Arduino pour capturer les données
├── classify_imu/              # Code Arduino pour classifier les gestes
├── serial_data_to_csv.py      # Script Python pour récupérer les données
├── arduino_tiny_ml.ipynb      # Notebook Jupyter pour entraîner le modèle
├── punch.csv                  # Données du geste "punch"
└── flex.csv                   # Données du geste "flex"
```

## ⚠️ Notes importantes

- **Normalisation** : Les données doivent être normalisées de la même manière à l'entraînement et sur Arduino
- **Nombre d'échantillons** : 119 échantillons par geste (doit être identique partout)
- **Port série** : Vérifier le port COM dans le Gestionnaire de périphériques Windows

## 🐛 Dépannage

- **Erreur port série** : Fermer le Moniteur série Arduino avant d'exécuter `serial_data_to_csv.py`
- **TensorFlow ne s'installe pas** : Utiliser Python 3.11 dans un environnement virtuel
- **Modèle ne fonctionne pas** : Vérifier que `model.h` est bien dans `classify_imu/`


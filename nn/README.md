# OCR (NN) – usage rapide

Contenu : `ocr_grid.c` + `nn_ocr.h` (MLP 2 couches sigmoïde) + `weights.txt` attendu + `grid_letters/` (tuiles binarisées).

Fichiers attendus :
- `weights.txt` dans `nn/` (format texte : `input_dim hidden_dim output_dim`, puis W1, b1, W2, b2).
- `grid_letters/` dans `nn/`, contenant des PNG nommés `row_col.png` ou `xCOL_yROW.png` (0/255).

Compiler (depuis `nn/`) :
```
gcc -Wall -Wextra -O2 -o ocr_grid ocr_grid.c -lm
```
(`make ocr_grid` fonctionne si `make` est dispo.)

Exécuter (toujours dans `nn/`) :
```
./ocr_grid           # utilise weights.txt et grid_letters/ par défaut
```
Résultats : `grille.txt` (lettres espacées) et `mots.txt` (lettres concaténées), générés dans `nn/`.

API (voir `nn_ocr.h`) :
- `nn_init(weights_path)`
- `nn_predict_letter_from_file(png_path)`
- `nn_process_grid(letters_dir, grille_path, mots_path)`
- `nn_shutdown()`

## Entraînement du réseau (Python)

Prérequis : `python3`, `pip install numpy pillow`.

1. Créer le dataset  
   - Place tes images de lettres binarisées dans `nn/dataset/train/A`, `.../B`, ..., `.../Z`.  
   - Toutes les images doivent avoir la même taille (w x h) et être binarisées (0/255).  
   - Le script infère `input_dim = w * h` sur la première image trouvée.

2. Générer un dataset de démo (optionnel)  
   - `python generate_font_dataset.py --out dataset/train --size 32 --per-font 2`  
   - Produit quelques PNG synthétiques A–Z (taille carrée 32x32 par défaut).

3. Entraîner le réseau et exporter les poids  
   - `python train_nn.py --data dataset/train --epochs 800 --lr 0.1 --hidden 64 --out weights.txt`  
   - `weights.txt` est écrit au format attendu par `ocr_grid.c` :
     ```
     input_dim hidden_dim output_dim
     <W1 flatten hidden_dim * input_dim>
     <b1 hidden_dim>
     <W2 flatten output_dim * hidden_dim>
     <b2 output_dim>
     ```
     - W1/W2 aplatis en row-major (ligne = neurone).
     - Mapping classes : 0->A, 1->B, ..., 25->Z.

4. Tester sur une grille découpée  
   - Compiler : `gcc -Wall -Wextra -O2 -o ocr_grid ocr_grid.c -lm`  
   - Préparer `grid_letters/` avec les tuiles `row_col.png` ou `xCOL_yROW.png`.  
   - Exécuter : `./ocr_grid`  
   - Vérifier `grille.txt` et `mots.txt`.

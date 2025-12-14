
import argparse
import math
import os
import sys
from pathlib import Path

import numpy as np
from PIL import Image


LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-x))


def load_dataset(data_root: Path, threshold: float):
    xs = []
    ys = []
    tile_size = None
    for idx, letter in enumerate(LETTERS):
        letter_dir = data_root / letter
        if not letter_dir.is_dir():
            continue
        for png in sorted(letter_dir.glob("*.png")):
            img = Image.open(png).convert("L")
            w, h = img.size
            if tile_size is None:
                tile_size = (w, h)
            elif tile_size != (w, h):
                raise ValueError(f"Toutes les images doivent avoir la même taille. {png} est {w}x{h}, attendu {tile_size}")
            arr = np.array(img, dtype=np.float32) / 255.0
            arr = (arr > threshold).astype(np.float32)
            xs.append(arr.flatten())
            y = np.zeros(len(LETTERS), dtype=np.float32)
            y[idx] = 1.0
            ys.append(y)
    if not xs:
        raise ValueError(f"Aucune image trouvée dans {data_root}. Assure-toi d'avoir des sous-dossiers A/ ... Z/ avec des PNG.")
    X = np.stack(xs, axis=0)
    Y = np.stack(ys, axis=0)
    return X, Y, tile_size


def train(X, Y, hidden_dim, epochs, lr):
    n_samples, input_dim = X.shape
    output_dim = Y.shape[1]

    rng = np.random.default_rng(seed=42)
    W1 = rng.normal(scale=0.1, size=(hidden_dim, input_dim)).astype(np.float32)
    b1 = np.zeros((hidden_dim,), dtype=np.float32)
    W2 = rng.normal(scale=0.1, size=(output_dim, hidden_dim)).astype(np.float32)
    b2 = np.zeros((output_dim,), dtype=np.float32)

    for epoch in range(1, epochs + 1):
        Z1 = X @ W1.T + b1
        A1 = sigmoid(Z1)
        Z2 = A1 @ W2.T + b2
        A2 = sigmoid(Z2)

        eps = 1e-7
        loss = -np.mean(Y * np.log(A2 + eps) + (1.0 - Y) * np.log(1.0 - A2 + eps))

        dZ2 = (A2 - Y) / n_samples
        dW2 = dZ2.T @ A1
        db2 = dZ2.sum(axis=0)
        dA1 = dZ2 @ W2
        dZ1 = dA1 * A1 * (1.0 - A1)
        dW1 = dZ1.T @ X
        db1 = dZ1.sum(axis=0)

        W1 -= lr * dW1
        b1 -= lr * db1
        W2 -= lr * dW2
        b2 -= lr * db2

        if epoch % max(1, epochs // 10) == 0 or epoch == 1:
            print(f"[{epoch}/{epochs}] loss={loss:.4f}")

    return W1, b1, W2, b2


def save_weights(weights_path: Path, W1, b1, W2, b2):
    input_dim = W1.shape[1]
    hidden_dim = W1.shape[0]
    output_dim = W2.shape[0]
    with weights_path.open("w", encoding="utf-8") as f:
        f.write(f"{input_dim} {hidden_dim} {output_dim}\n")
        f.write(" ".join(map(str, W1.reshape(-1))) + "\n")
        f.write(" ".join(map(str, b1.reshape(-1))) + "\n")
        f.write(" ".join(map(str, W2.reshape(-1))) + "\n")
        f.write(" ".join(map(str, b2.reshape(-1))) + "\n")
    print(f"Écrit {weights_path} (input_dim={input_dim}, hidden_dim={hidden_dim}, output_dim={output_dim})")


def parse_args():
    p = argparse.ArgumentParser(description="Entraîner un MLP OCR lettres et exporter weights.txt compatible C.")
    p.add_argument("--data", default="dataset/train", help="Dossier racine du dataset (A/..Z/).")
    p.add_argument("--hidden", type=int, default=64, help="Taille couche cachée.")
    p.add_argument("--epochs", type=int, default=2000, help="Nombre d'epochs (défaut 2000 pour un entraînement plus long).")
    p.add_argument("--lr", type=float, default=0.1, help="Learning rate.")
    p.add_argument("--threshold", type=float, default=0.5, help="Seuil de binarisation (0..1).")
    p.add_argument("--out", default="weights.txt", help="Chemin du fichier de poids généré.")
    return p.parse_args()


def main():
    args = parse_args()
    data_root = Path(args.data)
    try:
        X, Y, tile_size = load_dataset(data_root, threshold=args.threshold)
    except Exception as e:
        print(f"Erreur dataset: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Dataset: {X.shape[0]} images, taille tuile={tile_size[0]}x{tile_size[1]}, input_dim={X.shape[1]}")
    W1, b1, W2, b2 = train(X, Y, hidden_dim=args.hidden, epochs=args.epochs, lr=args.lr)
    save_weights(Path(args.out), W1, b1, W2, b2)


if __name__ == "__main__":
    main()

import math
import random


def sigmoid(x):
    return 1.0 / (1.0 + math.exp(-x))

def dsigmoid_output(a):
    return a * (1.0 - a)

def dot(M, v):
    resultat = []
    for row in M:
        s = 0.0
        for i in range(len(v)):
            s += row[i] * v[i]
        resultat.append(s)
    return resultat

def add(a, b):
    result = []

    i = 0
    while i < len(a):
        somme = a[i] + b[i]
        result.append(somme)
        i = i + 1
    return result

def mat_mult(a, b):
    result = []
    i = 0

    while i < len(a):
        produit = a[i] * b[i]
        result.append(produit)
        i = i + 1

    return result

def parametres():
    random.seed(0)
    W1 = [
        [random.uniform(-0.5, 0.5), random.uniform(-0.5, 0.5)],
        [random.uniform(-0.5, 0.5), random.uniform(-0.5, 0.5)]
    ]
    b1 = [0.0, 0.0]
    W2 = [[random.uniform(-0.5, 0.5), random.uniform(-0.5, 0.5)]]
    b2 = [0.0]

    return {"W1": W1, "b1": b1, "W2": W2, "b2": b2}

def forward(x, params):
    W1 = params["W1"]
    b1 = params["b1"]
    W2 = params["W2"]
    b2 = params["b2"]
    z1_0 = W1[0][0] * x[0] + W1[0][1] * x[1] + b1[0]
    z1_1 = W1[1][0] * x[0] + W1[1][1] * x[1] + b1[1]
    a1_0 = sigmoid(z1_0)
    a1_1 = sigmoid(z1_1)
    a1 = [a1_0, a1_1]
    z2 = W2[0][0] * a1[0] + W2[0][1] * a1[1] + b2[0]
    a2 = sigmoid(z2)
    cache = {
        "x": x, 
        "a1": a1,     
        "y_hat": [a2]   
    }
    return [a2], cache


def mse_loss(y_hat, y):
    diff = y_hat[0] - y[0]
    loss = 0.5 * diff * diff
    dL_dyhat = [diff]
    return loss, dL_dyhat

def backward(cache, params, dL_dyhat):
    x = cache["x"]
    a1 = cache["a1"]
    y_hat = cache["y_hat"]

    dyhat_dz2 = dsigmoid_output(y_hat[0])
    dL_dz2 = dL_dyhat[0] * dyhat_dz2

    dW2 = [[dL_dz2 * a1[0], dL_dz2 * a1[1]]]
    db2 = [dL_dz2]

    dL_da1 = [params["W2"][0][0] * dL_dz2, params["W2"][0][1] * dL_dz2]
    da1_dz1 = [dsigmoid_output(a1[0]), dsigmoid_output(a1[1])]
    dL_dz1 = mat_mult(dL_da1, da1_dz1)

    dW1 = [
        [dL_dz1[0] * x[0], dL_dz1[0] * x[1]],
        [dL_dz1[1] * x[0], dL_dz1[1] * x[1]],
    ]
    db1 = [dL_dz1[0], dL_dz1[1]]

    return {"dW1": dW1, "db1": db1, "dW2": dW2, "db2": db2}

def apply_grads(params, grads, lr):
    for r in range(2):
        for c in range(2):
            params["W1"][r][c] -= lr * grads["dW1"][r][c]
    for i in range(2):
        params["b1"][i] -= lr * grads["db1"][i]
    for c in range(2):
        params["W2"][0][c] -= lr * grads["dW2"][0][c]
    params["b2"][0] -= lr * grads["db2"][0]

# TESTS

def train_xor(epochs, lr=0.5):
    X = [[0,0], [0,1], [1,0], [1,1]]
    Y = [[0],   [1],   [1],   [0]]

    params = parametres()
    loss_history = []

    for _ in range(epochs):
        epoch_loss = 0.0
        for i in range(4):
            y_hat, cache = forward(X[i], params)
            loss, dL_dyhat = mse_loss(y_hat, Y[i])
            grads = backward(cache, params, dL_dyhat)
            apply_grads(params, grads, lr)
            epoch_loss += loss
        loss_history.append(epoch_loss / 4.0)
    return params, loss_history

def predict_value(x, params):
    y_hat, _ = forward(x, params)
    return y_hat[0]

# EXEC DU NN

if __name__ == "__main__":
    try:
        epochs = int(input("Combien d'epochs pour entraîner ? (ex: 5000) : "))
    except:
        epochs = 5000
        print("Entrée invalide, j'utilise 5000 epochs.")

    lr = 0.5
    params, history = train_xor(epochs, lr)

    print("\nRésultats (sorties réelles entre 0 et 1) :")
    tests = [[0,0],[0,1],[1,0],[1,1]]
    for x in tests:
        y = predict_value(x, params)
        print(f"{x} -> {y}")

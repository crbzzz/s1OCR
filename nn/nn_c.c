#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

typedef struct {
    double W1[2][2];
    double b1[2];
    double W2[1][2];
    double b2[1];
} Params;

typedef struct {
    double dW1[2][2];
    double db1[2];
    double dW2[1][2];
    double db2[1];
} Grads;

double rand_uniform() 
{ 
    return ((double)rand() / (double)RAND_MAX) - 0.5; 
}

double sigmoid(double x) 
{ 
    return 1.0 / (1.0 + exp(-x));
}

double dsigmoid_from_output(double a) 
{ 
    return a * (1.0 - a); 
}

void init_params(Params* p) 
{
    p->W1[0][0] = rand_uniform(); p->W1[0][1] = rand_uniform();
    p->W1[1][0] = rand_uniform(); p->W1[1][1] = rand_uniform();
    p->b1[0] = 0.0; p->b1[1] = 0.0;
    p->W2[0][0] = rand_uniform(); p->W2[0][1] = rand_uniform();
    p->b2[0] = 0.0;
}

void forward(const double x[2], const Params* p, double a1[2], double* y_hat) 
{
    double z1_0 = p->W1[0][0]*x[0] + p->W1[0][1]*x[1] + p->b1[0];
    double z1_1 = p->W1[1][0]*x[0] + p->W1[1][1]*x[1] + p->b1[1];
    a1[0] = sigmoid(z1_0);
    a1[1] = sigmoid(z1_1);
    double z2 = p->W2[0][0]*a1[0] + p->W2[0][1]*a1[1] + p->b2[0];
    *y_hat = sigmoid(z2);
}

void bce_loss(double y_hat, double y, double* loss, double* dL_dyhat) 
{
    double eps = 1e-12;
    double p = y_hat;
    if (p < eps) p = eps;
    if (p > 1.0 - eps) p = 1.0 - eps;
    *loss = -(y * log(p) + (1.0 - y) * log(1.0 - p));
    *dL_dyhat = y_hat - y;
}

void backward(const double x[2], const double a1[2], double y_hat, const Params* p, double dL_dyhat, Grads* g) 
{
    double dL_dz2 = dL_dyhat;
    g->dW2[0][0] = dL_dz2 * a1[0];
    g->dW2[0][1] = dL_dz2 * a1[1];
    g->db2[0] = dL_dz2;
    double dL_da1_0 = p->W2[0][0] * dL_dz2;
    double dL_da1_1 = p->W2[0][1] * dL_dz2;
    double dL_dz1_0 = dL_da1_0 * dsigmoid_from_output(a1[0]);
    double dL_dz1_1 = dL_da1_1 * dsigmoid_from_output(a1[1]);
    g->dW1[0][0] = dL_dz1_0 * x[0];
    g->dW1[0][1] = dL_dz1_0 * x[1];
    g->dW1[1][0] = dL_dz1_1 * x[0];
    g->dW1[1][1] = dL_dz1_1 * x[1];
    g->db1[0] = dL_dz1_0;
    g->db1[1] = dL_dz1_1;
}

void apply_grads(Params* p, const Grads* g, double lr) 
{
    p->W2[0][0] -= lr * g->dW2[0][0];
    p->W2[0][1] -= lr * g->dW2[0][1];
    p->b2[0]    -= lr * g->db2[0];
    p->W1[0][0] -= lr * g->dW1[0][0];
    p->W1[0][1] -= lr * g->dW1[0][1];
    p->W1[1][0] -= lr * g->dW1[1][0];
    p->W1[1][1] -= lr * g->dW1[1][1];
    p->b1[0]    -= lr * g->db1[0];
    p->b1[1]    -= lr * g->db1[1];
}

double predict_value(const double x[2], const Params* p) 
{
    double a1[2], y_hat;
    forward(x, p, a1, &y_hat);
    return y_hat;
}

void shuffle4(int idx[4]) 
{
    for (int i = 3; i > 0; i--) 
    {
        int j = rand() % (i + 1);
        int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
    }
}

void train_xor(int epochs, double lr, Params* p) 
{
    double X[4][2] = { {0,0}, {0,1}, {1,0}, {1,1} };
    double Y[4] = { 0, 1, 1, 0 };
    int order[4] = {0,1,2,3};
    Grads g;
    for (int e = 0; e < epochs; e++) 
    {
        shuffle4(order);
        for (int k = 0; k < 4; k++) 
        {
            int i = order[k];
            double a1[2], y_hat, loss, dL_dyhat;
            forward(X[i], p, a1, &y_hat);
            bce_loss(y_hat, Y[i], &loss, &dL_dyhat);
            backward(X[i], a1, y_hat, p, dL_dyhat, &g);
            apply_grads(p, &g, lr);
        }
    }
}



int main() {
    srand((unsigned)time(NULL));
    Params params;
    init_params(&params);
    int epochs;
    printf("Combien d'epochs pour entraîner ? (ex: 5000) : ");
    if (scanf("%d", &epochs) != 1) {
        epochs = 5000;
        printf("Erreur, 5k époch utilisées.\n");
    }
    double lr = 0.1;
    train_xor(epochs, lr, &params);
    printf("\nRésultat :\n");
    double t0[2] = {0,0};
    double t1[2] = {0,1};
    double t2[2] = {1,0};
    double t3[2] = {1,1};
    printf("[0, 0] -> %f\n", predict_value(t0, &params));
    printf("[0, 1] -> %f\n", predict_value(t1, &params));
    printf("[1, 0] -> %f\n", predict_value(t2, &params));
    printf("[1, 1] -> %f\n", predict_value(t3, &params));
    return 0;
}

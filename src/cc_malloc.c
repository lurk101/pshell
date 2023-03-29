#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <pico/stdlib.h>

#include "cc_malloc.h"

#define UDATA __attribute__((section(".ccudata")))

__attribute__((__noreturn__)) void run_fatal(const char* fmt, ...);

// Create node
struct node {
    struct node* left;
    struct node* right;
    int height;
    void* key;
};

static struct node* root UDATA;

// Calculate height
static int height(struct node* N) {
    if (N == NULL)
        return 0;
    return N->height;
}

static int max(int a, int b) { return (a > b) ? a : b; }

// Create a node
static struct node* newnode(void* key) {
    struct node* node = (struct node*)malloc(sizeof(struct node));
    if (node == 0)
        run_fatal("Out of memory");
    node->key = key;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    return node;
}

// Right rotate
static struct node* rightRotate(struct node* y) {
    struct node* x = y->left;
    struct node* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;
    return x;
}

// Left rotate
static struct node* leftRotate(struct node* x) {
    struct node* y = x->right;
    struct node* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = max(height(x->left), height(x->right)) + 1;
    y->height = max(height(y->left), height(y->right)) + 1;
    return y;
}

// Get the balance factor
static int getBalance(struct node* N) {
    if (N == NULL)
        return 0;
    return height(N->left) - height(N->right);
}

// Insert node
static struct node* insertnode(struct node* node, void* key) {
    // Find the correct position to insertnode the node and insertnode it
    if (node == NULL)
        return (newnode(key));
    if (key < node->key)
        node->left = insertnode(node->left, key);
    else if (key > node->key)
        node->right = insertnode(node->right, key);
    else
        return node;
    // Update the balance factor of each node and
    // Balance the tree
    node->height = 1 + max(height(node->left), height(node->right));
    int balance = getBalance(node);
    if (balance > 1) {
        if (key < node->left->key)
            return rightRotate(node);
        if (key > node->left->key) {
            node->left = leftRotate(node->left);
            return rightRotate(node);
        }
    }
    if (balance < -1) {
        if (key > node->right->key)
            return leftRotate(node);
        if (key < node->right->key) {
            node->right = rightRotate(node->right);
            return leftRotate(node);
        }
    }
    return node;
}

static struct node* minValuenode(struct node* node) {
    struct node* current = node;
    while (current->left != NULL)
        current = current->left;
    return current;
}

// Delete a nodes
static struct node* deletenode(struct node* root, void* key) {
    // Find the node and delete it
    if (root == NULL)
        return root;
    if (key < root->key)
        root->left = deletenode(root->left, key);
    else if (key > root->key)
        root->right = deletenode(root->right, key);
    else {
        if ((root->left == NULL) || (root->right == NULL)) {
            struct node* temp = root->left ? root->left : root->right;
            if (temp == NULL) {
                temp = root;
                root = NULL;
            } else
                *root = *temp;
            free(temp);
        } else {
            struct node* temp = minValuenode(root->right);
            root->key = temp->key;
            root->right = deletenode(root->right, temp->key);
        }
    }
    if (root == NULL)
        return root;
    root->height = 1 + max(height(root->left), height(root->right));
    int balance = getBalance(root);
    if (balance > 1) {
        if (getBalance(root->left) >= 0)
            return rightRotate(root);
        if (getBalance(root->left) < 0) {
            root->left = leftRotate(root->left);
            return rightRotate(root);
        }
    }
    if (balance < -1) {
        if (getBalance(root->right) <= 0)
            return leftRotate(root);
        if (getBalance(root->right) > 0) {
            root->right = rightRotate(root->right);
            return leftRotate(root);
        }
    }
    return root;
}

static struct node* searchnode(struct node* node, void* key) {
    struct node* n = node;
    while (n) {
        if (key == n->key)
            return n;
        if (key < n->key)
            n = n->left;
        else
            n = n->right;
    }
    return 0;
}

static void free_all(struct node* N) {
    if (N->right)
        free_all(N->right);
    if (N->left)
        free_all(N->left);
    free(N->key);
    free(N);
}

void* cc_malloc(int nbytes, int zero) {
    void* m = (struct node*)malloc(nbytes);
    if (m == 0)
        run_fatal("Out of memory");
    if (zero)
        memset(m, 0, nbytes);
    root = insertnode(root, m);
    return m;
}

void cc_free(void* m) {
    if (searchnode(root, m) == 0)
        run_fatal("Freeing unallocated memory");
    root = deletenode(root, m);
    free(m);
}

void cc_free_all(void) {
    if (root)
        free_all(root);
    root = 0;
}

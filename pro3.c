#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdint.h>

#define MAX_TREE_HT 100
#define NUM_THREADS 2

// Structure for Huffman tree node
struct MinHeapNode {
    char data;
    unsigned freq;
    struct MinHeapNode *left, *right;
};

// Min Heap structure
struct MinHeap {
    unsigned size;
    unsigned capacity;
    struct MinHeapNode **array;
};

// Shared frequency array
int freq[256] = {0};
pthread_mutex_t lock;

// Function to get file size
off_t getFileSize(const char* filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// Function to create a new node
struct MinHeapNode* newNode(char data, unsigned freq) {
    struct MinHeapNode* temp = (struct MinHeapNode*)malloc(sizeof(struct MinHeapNode));
    temp->left = temp->right = NULL;
    temp->data = data;
    temp->freq = freq;
    return temp;
}

// Function to create a min heap
struct MinHeap* createMinHeap(unsigned capacity) {
    struct MinHeap* minHeap = (struct MinHeap*)malloc(sizeof(struct MinHeap));
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->array = (struct MinHeapNode**)malloc(minHeap->capacity * sizeof(struct MinHeapNode*));
    return minHeap;
}

// Swap two min heap nodes
void swapMinHeapNode(struct MinHeapNode** a, struct MinHeapNode** b) {
    struct MinHeapNode* t = *a;
    *a = *b;
    *b = t;
}

// Min Heapify function
void minHeapify(struct MinHeap* minHeap, int idx) {
    int smallest = idx;
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;

    if (left < minHeap->size && minHeap->array[left]->freq < minHeap->array[smallest]->freq)
        smallest = left;

    if (right < minHeap->size && minHeap->array[right]->freq < minHeap->array[smallest]->freq)
        smallest = right;

    if (smallest != idx) {
        swapMinHeapNode(&minHeap->array[smallest], &minHeap->array[idx]);
        minHeapify(minHeap, smallest);
    }
}

// Extract minimum node from min heap
struct MinHeapNode* extractMin(struct MinHeap* minHeap) {
    struct MinHeapNode* temp = minHeap->array[0];
    minHeap->array[0] = minHeap->array[minHeap->size - 1];
    --minHeap->size;
    minHeapify(minHeap, 0);
    return temp;
}

// Insert node into min heap
void insertMinHeap(struct MinHeap* minHeap, struct MinHeapNode* minHeapNode) {
    ++minHeap->size;
    int i = minHeap->size - 1;

    while (i && minHeapNode->freq < minHeap->array[(i - 1) / 2]->freq) {
        minHeap->array[i] = minHeap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }

    minHeap->array[i] = minHeapNode;
}

// Build a Huffman Tree
struct MinHeapNode* buildHuffmanTree(int freq[]) {
    struct MinHeap* minHeap = createMinHeap(256);
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0)
            minHeap->array[minHeap->size++] = newNode(i, freq[i]);
    }

    while (minHeap->size > 1) {
        struct MinHeapNode *left = extractMin(minHeap);
        struct MinHeapNode *right = extractMin(minHeap);
        struct MinHeapNode *top = newNode('$', left->freq + right->freq);
        top->left = left;
        top->right = right;
        insertMinHeap(minHeap, top);
    }
    
    return extractMin(minHeap);
}

// Generate Huffman codes
void generateCodes(struct MinHeapNode* root, char* code, int top, char codes[256][MAX_TREE_HT]) {
    if (root->left) {
        code[top] = '0';
        generateCodes(root->left, code, top + 1, codes);
    }

    if (root->right) {
        code[top] = '1';
        generateCodes(root->right, code, top + 1, codes);
    }

    if (!root->left && !root->right) {
        code[top] = '\0';
        strcpy(codes[(unsigned char)root->data], code);
    }
}

// Count character frequencies
void countFrequencies(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error opening file for frequency count.\n");
        return;
    }

    char ch;
    while ((ch = fgetc(file)) != EOF) {
        freq[(unsigned char)ch]++;
    }

    fclose(file);
}

// Encode file and write to binary & text
void encodeFile(const char* inputFile, const char* outputFileBin, const char* outputFileTxt, char codes[256][MAX_TREE_HT]) {
    FILE *in = fopen(inputFile, "r");
    FILE *outBin = fopen(outputFileBin, "wb");
    FILE *outTxt = fopen(outputFileTxt, "w");

    if (!in || !outBin || !outTxt) {
        printf("Error opening files.\n");
        return;
    }
    
    uint8_t buffer = 0;
    int bitCount = 0;
    char ch;
    
    while ((ch = fgetc(in)) != EOF) {
        for (char* p = codes[(unsigned char)ch]; *p; p++) {
            buffer = (buffer << 1) | (*p - '0');
            bitCount++;
            fputc(*p, outTxt); // Write each bit as text to the txt file

            if (bitCount == 8) {
                fwrite(&buffer, 1, 1, outBin);
                bitCount = 0;
                buffer = 0;
            }
        }
    }

    if (bitCount > 0) {
        buffer <<= (8 - bitCount);
        fwrite(&buffer, 1, 1, outBin);
    }

    fclose(in);
    fclose(outBin);
    fclose(outTxt);
}

// Main function
int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    countFrequencies(argv[1]);
    
    off_t originalSize = getFileSize(argv[1]);
    printf("Original File Size: %ld bytes\n", originalSize);

    struct MinHeapNode* root = buildHuffmanTree(freq);
    char codes[256][MAX_TREE_HT] = {0};
    char code[MAX_TREE_HT];
    generateCodes(root, code, 0, codes);

    encodeFile(argv[1], "compressed.bin", "compressed.txt", codes);

    off_t compressedSize = getFileSize("compressed.bin");
    printf("Compressed File Size: %ld bytes\n", compressedSize);
    printf("Compression Reduction: %.2f%%\n", ((double)(originalSize - compressedSize) / originalSize) * 100);

    return 0;
}

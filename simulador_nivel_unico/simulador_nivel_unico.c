#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

// ============================================================================
// PARÂMETROS DA ARQUITETURA SIMULADA (NÍVEL ÚNICO)
// ============================================================================
#define VIRTUAL_ADDRESS_BITS 16
#define PAGE_SIZE 256
#define PHYSICAL_MEMORY_SIZE 4096 // 4 KB de RAM
#define TLB_ENTRIES 4

// TEMPOS DE ACESSO
#define TLB_ACCESS_TIME 1
#define MEMORY_ACCESS_TIME 100
#define DISK_ACCESS_TIME 50000

// CÁLCULOS DERIVADOS
#define OFFSET_BITS 8
#define VPN_BITS (VIRTUAL_ADDRESS_BITS - OFFSET_BITS)
#define NUM_VIRTUAL_PAGES (1 << VPN_BITS) // 2^8 = 256 páginas virtuais
#define NUM_PHYSICAL_FRAMES (PHYSICAL_MEMORY_SIZE / PAGE_SIZE) // 16 quadros
#define OFFSET_MASK (PAGE_SIZE - 1)

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

typedef struct {
    bool valid;
    int frame_number;
} PageTableEntry;

typedef struct {
    bool valid;
    int vpn;
    int frame_number;
} TLBEntry;

// ============================================================================
// ESTADO GLOBAL DO SISTEMA
// ============================================================================
PageTableEntry page_table[NUM_VIRTUAL_PAGES]; // Tabela de Nível Único
TLBEntry tlb[TLB_ENTRIES];
bool is_frame_free[NUM_PHYSICAL_FRAMES];
int frame_to_vpn_map[NUM_PHYSICAL_FRAMES];
int fifo_victim_frame_ptr = 0;
int tlb_victim_entry_ptr = 0;

// Métricas de Desempenho
long long total_accesses = 0, tlb_hits = 0, tlb_misses = 0, page_faults = 0, total_access_time = 0;

// ============================================================================
// DECLARAÇÃO DE FUNÇÕES
// ============================================================================
void initialize_system();
int find_free_frame();
void update_tlb(int vpn, int frame_number);
void handle_page_fault(int vpn);
void translate_address(unsigned int virtual_address);
void print_summary_report();

// ============================================================================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_de_enderecos>\n", argv[0]);
        return 1;
    }
    FILE* file = fopen(argv[1], "r");
    if (!file) {
        perror("Erro ao abrir o arquivo de entrada");
        return 1;
    }
    initialize_system();
    unsigned int virtual_address;
    while (fscanf(file, "%u", &virtual_address) == 1) {
        translate_address(virtual_address);
    }
    fclose(file);
    print_summary_report();
    return 0;
}

void initialize_system() {
    for (int i = 0; i < NUM_VIRTUAL_PAGES; i++) {
        page_table[i].valid = false;
    }
    for (int i = 0; i < TLB_ENTRIES; i++) {
        tlb[i].valid = false;
    }
    for (int i = 0; i < NUM_PHYSICAL_FRAMES; i++) {
        is_frame_free[i] = true;
        frame_to_vpn_map[i] = -1;
    }
    printf("Sistema (Nivel Unico) inicializado. %d quadros fisicos disponiveis.\n\n", NUM_PHYSICAL_FRAMES);
}

int find_free_frame() {
    for (int i = 0; i < NUM_PHYSICAL_FRAMES; i++) {
        if (is_frame_free[i]) return i;
    }
    return -1;
}

void update_tlb(int vpn, int frame_number) {
    tlb[tlb_victim_entry_ptr].valid = true;
    tlb[tlb_victim_entry_ptr].vpn = vpn;
    tlb[tlb_victim_entry_ptr].frame_number = frame_number;
    tlb_victim_entry_ptr = (tlb_victim_entry_ptr + 1) % TLB_ENTRIES;
}

void handle_page_fault(int vpn) {
    page_faults++;
    total_access_time += DISK_ACCESS_TIME;
    printf("--> FALTA DE PAGINA (Page Fault) para a pagina virtual %d!\n", vpn);

    int frame_to_use = find_free_frame();
    if (frame_to_use == -1) {
        frame_to_use = fifo_victim_frame_ptr;
        printf("    Nenhum quadro livre. Substituindo pagina no quadro %d.\n", frame_to_use);
        fifo_victim_frame_ptr = (fifo_victim_frame_ptr + 1) % NUM_PHYSICAL_FRAMES;
        int old_vpn = frame_to_vpn_map[frame_to_use];
        page_table[old_vpn].valid = false;
    }

    printf("    Carregando pagina virtual %d para o quadro fisico %d.\n", vpn, frame_to_use);
    page_table[vpn].valid = true;
    page_table[vpn].frame_number = frame_to_use;
    is_frame_free[frame_to_use] = false;
    frame_to_vpn_map[frame_to_use] = vpn;

    update_tlb(vpn, frame_to_use);
}

void translate_address(unsigned int virtual_address) {
    total_accesses++;
    printf("Traduzindo endereco virtual: %u\n", virtual_address);

    int vpn = virtual_address >> OFFSET_BITS;
    int offset = virtual_address & OFFSET_MASK;

    printf("1. Divisao do Endereco:\n   VPN: %d, Offset: %d\n", vpn, offset);

    total_access_time += TLB_ACCESS_TIME;
    int frame_number = -1;
    bool tlb_hit = false;
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (tlb[i].valid && tlb[i].vpn == vpn) {
            frame_number = tlb[i].frame_number;
            tlb_hit = true;
            tlb_hits++;
            printf("--> Acerto no TLB (TLB Hit)!\n");
            break;
        }
    }

    if (!tlb_hit) {
        tlb_misses++;
        printf("--> Falha no TLB (TLB Miss). Verificando Tabela de Paginas...\n");
        
        total_access_time += MEMORY_ACCESS_TIME; // UM acesso a RAM para a tabela de nivel unico
        if (page_table[vpn].valid) {
            frame_number = page_table[vpn].frame_number;
            printf("    Pagina encontrada na memoria. Atualizando TLB.\n");
            update_tlb(vpn, frame_number);
        } else {
            handle_page_fault(vpn);
            frame_number = page_table[vpn].frame_number;
        }
    }

    unsigned int physical_address = (frame_number << OFFSET_BITS) | offset;
    total_access_time += MEMORY_ACCESS_TIME;
    printf("    Endereco Fisico Resultante: %u (Quadro: %d, Offset: %d)\n\n",
           physical_address, frame_number, offset);
}

void print_summary_report() {
    double tlb_hit_ratio = (total_accesses > 0) ? (double)tlb_hits / total_accesses * 100.0 : 0.0;
    double effective_access_time = (total_accesses > 0) ? (double)total_access_time / total_accesses : 0.0;
    
    printf("\n========================================================\n");
    printf("          RELATORIO FINAL (SIMULACAO NIVEL UNICO)\n");
    printf("========================================================\n");
    printf("Total de Acessos a Memoria: %lld\n", total_accesses);
    printf("Metricas do TLB: Hits=%lld, Misses=%lld, Taxa de Acerto=%.2f%%\n", tlb_hits, tlb_misses, tlb_hit_ratio);
    printf("Metricas de Paginacao: Faltas de Pagina=%lld\n", page_faults);
    printf("Metricas de Desempenho: EAT=%.2f unidades\n", effective_access_time);
    printf("========================================================\n");
}
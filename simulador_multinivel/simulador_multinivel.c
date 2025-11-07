#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

// ============================================================================
// PARÂMETROS DA ARQUITETURA SIMULADA (DOIS NÍVEIS)
// ============================================================================
#define VIRTUAL_ADDRESS_BITS 32
#define PAGE_SIZE 4096

// Divisão dos bits: 10 (Dir) + 10 (Tabela) + 12 (Offset)
#define OFFSET_BITS 12
#define PT_INDEX_BITS 10
#define PD_INDEX_BITS 10

// Máscaras para extrair cada parte do endereço
#define OFFSET_MASK 0xFFF
#define PT_INDEX_MASK 0x3FF
#define PD_INDEX_MASK 0x3FF

#define NUM_PD_ENTRIES (1 << PD_INDEX_BITS)
#define NUM_PT_ENTRIES (1 << PT_INDEX_BITS)

#define PHYSICAL_MEMORY_SIZE (64 * 1024) // 64 KB RAM
#define NUM_PHYSICAL_FRAMES (PHYSICAL_MEMORY_SIZE / PAGE_SIZE) // 16 quadros
#define TLB_ENTRIES 4

// TEMPOS DE ACESSO
#define TLB_ACCESS_TIME 1
#define MEMORY_ACCESS_TIME 100
#define DISK_ACCESS_TIME 50000

// ============================================================================
// ESTRUTURAS DE DADOS
// ============================================================================

typedef struct {
    bool valid;
    int frame_number;
} PageTableEntry;

typedef struct {
    bool valid;
    PageTableEntry* page_table;
} PageDirectoryEntry;

typedef struct {
    bool valid;
    int vpn;
    int frame_number;
} TLBEntry;

// ============================================================================
// ESTADO GLOBAL DO SISTEMA
// ============================================================================
PageDirectoryEntry page_directory[NUM_PD_ENTRIES];
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
void handle_page_fault(int vpn, int pd_index, int pt_index);
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
    for (int i = 0; i < NUM_PD_ENTRIES; i++) {
        if (page_directory[i].valid) {
            free(page_directory[i].page_table);
        }
    }
    return 0;
}

void initialize_system() {
    for (int i = 0; i < NUM_PD_ENTRIES; i++) {
        page_directory[i].valid = false;
        page_directory[i].page_table = NULL;
    }
    for (int i = 0; i < TLB_ENTRIES; i++) {
        tlb[i].valid = false;
    }
    for (int i = 0; i < NUM_PHYSICAL_FRAMES; i++) {
        is_frame_free[i] = true;
        frame_to_vpn_map[i] = -1;
    }
    printf("Sistema (Dois Niveis) inicializado. %d quadros fisicos disponiveis.\n\n", NUM_PHYSICAL_FRAMES);
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

void handle_page_fault(int vpn, int pd_index, int pt_index) {
    page_faults++;
    total_access_time += DISK_ACCESS_TIME;
    printf("--> FALTA DE PAGINA (Page Fault) para a pagina virtual %d!\n", vpn);

    if (!page_directory[pd_index].valid) {
        printf("    Criando tabela de paginas de segundo nivel para o indice de diretorio %d.\n", pd_index);
        page_directory[pd_index].page_table = (PageTableEntry*)malloc(sizeof(PageTableEntry) * NUM_PT_ENTRIES);
        if (page_directory[pd_index].page_table == NULL) {
            fprintf(stderr, "Falha ao alocar memoria para tabela de paginas!\n");
            exit(1);
        }
        for (int i = 0; i < NUM_PT_ENTRIES; i++) {
            page_directory[pd_index].page_table[i].valid = false;
        }
        page_directory[pd_index].valid = true;
    }

    int frame_to_use = find_free_frame();
    if (frame_to_use == -1) {
        frame_to_use = fifo_victim_frame_ptr;
        printf("    Nenhum quadro livre. Substituindo pagina no quadro %d.\n", frame_to_use);
        fifo_victim_frame_ptr = (fifo_victim_frame_ptr + 1) % NUM_PHYSICAL_FRAMES;
        int old_vpn = frame_to_vpn_map[frame_to_use];
        int old_pd_index = (old_vpn * PAGE_SIZE) >> (OFFSET_BITS + PT_INDEX_BITS);
        int old_pt_index = ((old_vpn * PAGE_SIZE) >> OFFSET_BITS) & PT_INDEX_MASK;
        page_directory[old_pd_index].page_table[old_pt_index].valid = false;
    }

    printf("    Carregando pagina virtual %d para o quadro fisico %d.\n", vpn, frame_to_use);
    page_directory[pd_index].page_table[pt_index].valid = true;
    page_directory[pd_index].page_table[pt_index].frame_number = frame_to_use;
    is_frame_free[frame_to_use] = false;
    frame_to_vpn_map[frame_to_use] = vpn;

    update_tlb(vpn, frame_to_use);
}

void translate_address(unsigned int virtual_address) {
    total_accesses++;
    printf("Traduzindo endereco virtual (32 bits): %u\n", virtual_address);

    int offset = virtual_address & OFFSET_MASK;
    int pt_index = (virtual_address >> OFFSET_BITS) & PT_INDEX_MASK;
    int pd_index = (virtual_address >> (OFFSET_BITS + PT_INDEX_BITS)) & PD_INDEX_MASK;
    int vpn = virtual_address >> OFFSET_BITS;

    printf("1. Divisao do Endereco:\n   PD_Index: %d, PT_Index: %d, Offset: %d\n", pd_index, pt_index, offset);

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
        printf("--> Falha no TLB (TLB Miss). Passeando pela hierarquia de tabelas...\n");

        total_access_time += MEMORY_ACCESS_TIME;
        if (!page_directory[pd_index].valid) {
            handle_page_fault(vpn, pd_index, pt_index);
            frame_number = page_directory[pd_index].page_table[pt_index].frame_number;
        } else {
            total_access_time += MEMORY_ACCESS_TIME;
            PageTableEntry* pte = &page_directory[pd_index].page_table[pt_index];
            if (pte->valid) {
                frame_number = pte->frame_number;
                printf("    Pagina encontrada na memoria. Atualizando TLB.\n");
                update_tlb(vpn, frame_number);
            } else {
                handle_page_fault(vpn, pd_index, pt_index);
                frame_number = pte->frame_number;
            }
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
    printf("         RELATORIO FINAL (SIMULACAO DOIS NIVEIS)\n");
    printf("========================================================\n");
    printf("Total de Acessos a Memoria: %lld\n", total_accesses);
    printf("Metricas do TLB: Hits=%lld, Misses=%lld, Taxa de Acerto=%.2f%%\n", tlb_hits, tlb_misses, tlb_hit_ratio);
    printf("Metricas de Paginacao: Faltas de Pagina=%lld\n", page_faults);
    printf("Metricas de Desempenho: EAT=%.2f unidades\n", effective_access_time);
    printf("========================================================\n");
}
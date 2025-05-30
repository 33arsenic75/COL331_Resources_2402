#ifndef PAGING_H
#define PAGING_H

void handle_pgfault();
pte_t* select_a_victim(pde_t *pgdir);
void clearaccessbit(pde_t *pgdir);
int getswappedblk(pde_t *pgdir, pte_t *pte);
int swap_page(pde_t *pgdir);
void swap_page_from_pte(pte_t *pte, pde_t *pgdir);
void map_address(pde_t *pgdir, uint addr);
pte_t *uva2pte(pde_t *pgdir, uint uva);
void initswapslots(void);

#endif

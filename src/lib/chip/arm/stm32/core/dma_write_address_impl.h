// Private completed-write accessor shared by the DMA backends and their register-level tests.
#pragma once

uintptr_t rulos_dma_get_write_address(const rulos_dma_channel_t *ch) {
  const int idx = state_to_idx(ch);
  const dma_channel_hw_t *hw = &g_hw[idx];
#if defined(RULOS_ARM_stm32h5)
  // RM0481 16.8.15: CDAR advances after each completed destination burst. Our bursts contain one
  // item, so this excludes source reads still in the GPDMA FIFO, unlike CBR1.BNDT.
  uintptr_t address = LL_DMA_GetDestAddress(hw->dma, hw->ll_channel);
#else
  // Classic DMA has no FIFO and leaves CMAR at the configured base. CNDTR advances only after
  // the peripheral read and memory write complete; reconstruct the next address in memory units.
  const uint32_t remaining = LL_DMA_GetDataLength(hw->dma, hw->ll_channel);
  const uint32_t width = LL_DMA_GetMemorySize(hw->dma, hw->ll_channel);
  const uint32_t bytes = width == LL_DMA_MDATAALIGN_WORD       ? 4
                         : width == LL_DMA_MDATAALIGN_HALFWORD ? 2
                                                               : 1;
  uintptr_t address = (uintptr_t)LL_DMA_GetMemoryAddress(hw->dma, hw->ll_channel) +
                      (g_state[idx].nitems - remaining) * bytes;
#endif
  // Publish the completed prefix before the caller reads its contents. Cache maintenance, when
  // applicable, remains the buffer owner's responsibility.
  __DMB();
  return address;
}

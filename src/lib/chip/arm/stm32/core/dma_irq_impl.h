// Private dispatcher shared by the classic DMA and GPDMA backends. Include
// after defining the channel tables, state, and channel-indexed flag helpers.
#pragma once

CCMRAM static void dispatch_half_transfer(const dma_channel_hw_t *hw, dma_channel_state_t *s) {
  if (ll_dma_is_active_flag_ht(hw->dma, hw->ll_channel) &&
      LL_DMA_IsEnabledIT_HT(hw->dma, hw->ll_channel)) {
    ll_dma_clear_flag_ht(hw->dma, hw->ll_channel);
    if (s->ht_callback) {
      s->ht_callback(s->user_data);
    }
  }
}

CCMRAM static void dispatch_channel_irq(int idx) {
  const dma_channel_hw_t *hw = &g_hw[idx];
  dma_channel_state_t *s = &g_state[idx];
  if (hw->dma == NULL || !s->allocated) {
    return;
  }

  // Snapshot before callbacks: a boundary reached during dispatch belongs to
  // the next IRQ. If both flags are pending, the current DMA half identifies
  // the latest boundary, even after more than one wrap. Lost wraps cannot be
  // recovered from flags, but the consumer's cursor must end in the right half.
  bool tc = ll_dma_is_active_flag_tc(hw->dma, hw->ll_channel);
  bool ht = ll_dma_is_active_flag_ht(hw->dma, hw->ll_channel) &&
            LL_DMA_IsEnabledIT_HT(hw->dma, hw->ll_channel);
  bool half_first = true;
  if (tc && ht && s->circular) {
    uint32_t remaining = rulos_dma_get_remaining((rulos_dma_channel_t *)s);
    half_first = remaining == 0 || remaining > s->nitems / 2;
  }

  if (ht && half_first) {
    dispatch_half_transfer(hw, s);
  }
  // Recheck the flag because an earlier callback may have stopped the channel.
  if (tc && ll_dma_is_active_flag_tc(hw->dma, hw->ll_channel)) {
    ll_dma_clear_flag_tc(hw->dma, hw->ll_channel);
    if (s->tc_callback) {
      s->tc_callback(s->user_data);
    }
  }
  if (ht && !half_first) {
    dispatch_half_transfer(hw, s);
  }

  if (ll_dma_is_active_flag_te(hw->dma, hw->ll_channel)) {
    ll_dma_clear_flag_te(hw->dma, hw->ll_channel);
    if (s->error_callback) {
      s->error_callback(s->user_data);
    }
  }
}

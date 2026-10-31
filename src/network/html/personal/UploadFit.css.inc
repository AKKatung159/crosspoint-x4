/* PERSONAL-HOOK(upload-fit): upload options (Optimize EPUB, Advanced Mode, PDF to XTC) wrap on
   narrow phones, so no control runs off the dialog. */
@media (max-width: 600px) {
  .convert-options { box-sizing: border-box; max-width: 100%; }
  .advanced-setting-row { flex-wrap: wrap; row-gap: 8px; }
  .setting-label { flex-shrink: 1; min-width: 0; }
  .advanced-setting-row > .setting-controls { flex: 1 1 100%; align-items: stretch; }
  .advanced-setting-row > .setting-label + .setting-controls { flex: 0 0 auto; }
  .quality-presets, .rotation-buttons, .overlap-selector { box-sizing: border-box; width: 100%; flex-wrap: wrap; }
  .quality-preset, .rotation-btn, .overlap-btn { flex: 1 1 0; justify-content: center; padding: 6px 4px; min-width: 52px; }
  .convert-settings { display: flex; flex-wrap: wrap; gap: 4px 10px; }
}

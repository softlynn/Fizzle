(() => {
  const btn = document.getElementById('downloadBtn');
  if (!btn) return;

  btn.addEventListener('mouseenter', () => {
    btn.style.boxShadow = '0 10px 28px rgba(126, 196, 255, 0.42)';
  });

  btn.addEventListener('mouseleave', () => {
    btn.style.boxShadow = '';
  });
})();

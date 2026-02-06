(() => {
  const reveals = document.querySelectorAll('.reveal');
  const io = new IntersectionObserver((entries) => {
    entries.forEach((entry, i) => {
      if (!entry.isIntersecting) return;
      setTimeout(() => entry.target.classList.add('show'), i * 45);
      io.unobserve(entry.target);
    });
  }, { threshold: 0.12 });

  reveals.forEach((el) => io.observe(el));

  const btn = document.getElementById('downloadBtn');
  if (btn) {
    btn.addEventListener('mousemove', (e) => {
      const r = btn.getBoundingClientRect();
      const x = e.clientX - r.left;
      const y = e.clientY - r.top;
      btn.style.background = `radial-gradient(circle at ${x}px ${y}px, #b4e6ff 0%, #7ec7ff 40%, #62b3ff 70%)`;
    });

    btn.addEventListener('mouseleave', () => {
      btn.style.background = 'linear-gradient(90deg, #64b3ff, #8ed3ff)';
    });
  }
})();

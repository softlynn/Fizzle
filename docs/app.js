(() => {
  const reveals = document.querySelectorAll('.reveal');
  const io = new IntersectionObserver((entries) => {
    entries.forEach((entry, i) => {
      if (!entry.isIntersecting) return;
      setTimeout(() => entry.target.classList.add('show'), i * 55);
      io.unobserve(entry.target);
    });
  }, { threshold: 0.14 });

  reveals.forEach((node) => io.observe(node));

  const cta = document.getElementById('downloadBtn');
  if (cta) {
    cta.addEventListener('mousemove', (event) => {
      const rect = cta.getBoundingClientRect();
      const x = event.clientX - rect.left;
      const y = event.clientY - rect.top;
      cta.style.background = `radial-gradient(circle at ${x}px ${y}px, #c4edff 0%, #89ccff 42%, #63b6ff 72%)`;
    });

    cta.addEventListener('mouseleave', () => {
      cta.style.background = 'linear-gradient(90deg, #6dbbff, #a4ddff)';
    });
  }

  const tiltCard = document.getElementById('tiltCard');
  if (tiltCard && window.matchMedia('(pointer:fine)').matches) {
    const damp = 12;

    tiltCard.addEventListener('mousemove', (event) => {
      const rect = tiltCard.getBoundingClientRect();
      const px = (event.clientX - rect.left) / rect.width;
      const py = (event.clientY - rect.top) / rect.height;

      const rx = (0.5 - py) * damp;
      const ry = (px - 0.5) * damp;

      tiltCard.style.transform = `perspective(900px) rotateX(${rx}deg) rotateY(${ry}deg) translateZ(0)`;
    });

    tiltCard.addEventListener('mouseleave', () => {
      tiltCard.style.transform = 'perspective(900px) rotateX(0deg) rotateY(0deg)';
    });
  }
})();

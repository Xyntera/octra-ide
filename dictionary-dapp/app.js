const terms = [
  {
    term: 'Octra',
    category: 'platform',
    summary: 'A privacy-oriented blockchain environment for secure data.',
    example: 'Octra is used here as the home for a dictionary-style dapp.',
  },
  {
    term: 'Devnet',
    category: 'network',
    summary: 'A development network for testing ideas before production.',
    example: 'Use devnet to prototype features and presentation layers.',
  },
  {
    term: 'Dictionary',
    category: 'app',
    summary: 'A structured collection of terms with definitions, examples, and metadata.',
    example: 'This page presents dictionary data in a read-only layout.',
  },
  {
    term: 'Frontend',
    category: 'interface',
    summary: 'The visible, interactive presentation layer of an application.',
    example: 'A frontend-only build keeps the experience simple and static.',
  },
  {
    term: 'Read-only',
    category: 'presentation',
    summary: 'Data can be viewed but not edited from the page.',
    example: 'This version removes wallet, status, and edit controls.',
  },
  {
    term: 'Schema',
    category: 'structure',
    summary: 'The shape of the data shown on screen.',
    example: 'Term, definition, example, and category are shown together.',
  }
];

const schema = [
  ['term', 'The word or phrase being presented.', 'Octra'],
  ['definition', 'A concise meaning or explanation.', 'Privacy-oriented blockchain'],
  ['example', 'A short usage line or context.', 'Octra is the base for this dapp.'],
  ['category', 'A small label that groups related terms.', 'platform'],
  ['source', 'Where the data belongs conceptually.', 'curated frontend content']
];

const chips = ['all', 'platform', 'network', 'app', 'interface', 'presentation', 'structure'];
let activeChip = 'all';

const $ = (id) => document.getElementById(id);

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>"']/g, (m) => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]));
}

function renderChips() {
  $('chips').innerHTML = chips.map((chip) => `
    <div class="chip ${chip === activeChip ? 'active' : ''}" data-chip="${chip}">${chip}</div>
  `).join('');
  document.querySelectorAll('.chip').forEach((chip) => {
    chip.addEventListener('click', () => {
      activeChip = chip.dataset.chip;
      render();
    });
  });
}

function renderMetrics(list) {
  const uniqueCats = new Set(list.map((item) => item.category)).size;
  $('metrics').innerHTML = [
    ['entries', list.length],
    ['categories', uniqueCats],
    ['mode', 'read-only'],
    ['layout', 'static']
  ].map(([label, value]) => `
    <div class="metric"><span>${label}</span><strong>${escapeHtml(value)}</strong></div>
  `).join('');
}

function renderCards(list) {
  if (list.length === 0) {
    $('cards').innerHTML = '<div class="empty">no matches for the current search</div>';
    return;
  }
  $('cards').innerHTML = list.map((item) => `
    <article class="card">
      <div class="card-top">
        <div class="term">${escapeHtml(item.term)}</div>
        <div class="badge">${escapeHtml(item.category)}</div>
      </div>
      <div class="definition">${escapeHtml(item.summary)}</div>
      <div class="example"><strong>Example:</strong> ${escapeHtml(item.example)}</div>
      <div class="meta">
        <span>read-only</span>
        <span>frontend</span>
        <span>presentation</span>
      </div>
    </article>
  `).join('');
}

function renderSchema() {
  $('schema').innerHTML = schema.map(([field, meaning, example]) => `
    <tr>
      <td>${escapeHtml(field)}</td>
      <td>${escapeHtml(meaning)}</td>
      <td>${escapeHtml(example)}</td>
    </tr>
  `).join('');
}

function render() {
  const q = $('search').value.trim().toLowerCase();
  const filtered = terms.filter((item) => {
    const matchesChip = activeChip === 'all' || item.category === activeChip;
    const haystack = [item.term, item.category, item.summary, item.example].join(' ').toLowerCase();
    const matchesQuery = !q || haystack.includes(q);
    return matchesChip && matchesQuery;
  });
  renderChips();
  renderMetrics(filtered);
  renderCards(filtered);
  renderSchema();
}

$('search').addEventListener('input', render);
render();

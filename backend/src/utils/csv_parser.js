function parseCSV(text) {
  const lines = [];
  let row = [];
  let inQuotes = false;
  let field = '';

  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === '"') {
      if (inQuotes && text[i + 1] === '"') {
        field += '"';
        i++;
      } else {
        inQuotes = !inQuotes;
      }
    } else if (c === ',' && !inQuotes) {
      row.push(field.trim());
      field = '';
    } else if ((c === '\r' || c === '\n') && !inQuotes) {
      if (c === '\r' && text[i + 1] === '\n') {
        i++;
      }
      row.push(field.trim());
      if (row.length > 0 && row.some(cell => cell.length > 0)) {
        lines.push(row);
      }
      row = [];
      field = '';
    } else {
      field += c;
    }
  }

  if (field || row.length > 0) {
    row.push(field.trim());
    if (row.some(cell => cell.length > 0)) {
      lines.push(row);
    }
  }

  return lines;
}

function normalizeHeader(str) {
  return String(str || '').toLowerCase().replace(/[^a-z0-9]/g, '');
}

function toCSVString(rows) {
  return rows.map(r => r.map(cell => {
    const s = String(cell == null ? '' : cell);
    if (s.includes(',') || s.includes('"') || s.includes('\n') || s.includes('\r')) {
      return `"${s.replace(/"/g, '""')}"`;
    }
    return s;
  }).join(',')).join('\n') + '\n';
}

module.exports = {
  parseCSV,
  normalizeHeader,
  toCSVString
};

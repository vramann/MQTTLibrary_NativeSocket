const ExcelJS = require('exceljs');
const db = require('../db');
const { computeHoldings, summary, dividendSummary } = require('./portfolio');

const MONEY = '#,##0.00';
const QTY = '#,##0.###';
const HEADER_FILL = { type: 'pattern', pattern: 'solid', fgColor: { argb: 'FF1F3A5F' } };

const CLASS_LABELS = {
  EQUITY: 'Equity',
  MUTUAL_FUND: 'Mutual Funds',
  GOV_BOND: 'Government Bonds',
  GOLD_BOND: 'Gold Bonds (SGB)',
  OTHER: 'Other',
};

function styleHeader(sheet) {
  const row = sheet.getRow(1);
  row.font = { bold: true, color: { argb: 'FFFFFFFF' } };
  row.eachCell((cell) => {
    cell.fill = HEADER_FILL;
    cell.alignment = { vertical: 'middle' };
  });
  sheet.views = [{ state: 'frozen', ySplit: 1 }];
}

function autoWidth(sheet) {
  sheet.columns.forEach((col) => {
    let max = 10;
    col.eachCell({ includeEmpty: false }, (cell) => {
      const len = String(cell.value ?? '').length;
      if (len > max) max = len;
    });
    col.width = Math.min(max + 2, 45);
  });
}

function addHoldingsSheet(wb, assetClass) {
  const rows = computeHoldings(assetClass).filter((h) => h.quantity > 0 || h.realized_pnl !== 0);
  const sheet = wb.addWorksheet(assetClass ? CLASS_LABELS[assetClass] : 'All Holdings');
  const isBond = assetClass === 'GOV_BOND' || assetClass === 'GOLD_BOND';
  sheet.columns = [
    { header: 'Symbol', key: 'symbol' },
    { header: 'Name', key: 'name' },
    ...(assetClass ? [] : [{ header: 'Asset Class', key: 'asset_class' }]),
    ...(isBond
      ? [
          { header: 'Coupon %', key: 'coupon_rate' },
          { header: 'Maturity', key: 'maturity_date' },
        ]
      : []),
    { header: 'Quantity', key: 'quantity', style: { numFmt: QTY } },
    { header: 'Avg Cost', key: 'avg_cost', style: { numFmt: MONEY } },
    { header: 'Invested (₹)', key: 'invested', style: { numFmt: MONEY } },
    { header: 'Current Price', key: 'current_price', style: { numFmt: MONEY } },
    { header: 'Current Value (₹)', key: 'current_value', style: { numFmt: MONEY } },
    { header: 'Unrealized P&L (₹)', key: 'unrealized_pnl', style: { numFmt: MONEY } },
    { header: 'P&L %', key: 'unrealized_pnl_pct', style: { numFmt: '0.00' } },
    { header: 'Realized P&L (₹)', key: 'realized_pnl', style: { numFmt: MONEY } },
    { header: 'Income Received (₹)', key: 'dividends_received', style: { numFmt: MONEY } },
  ];
  for (const h of rows) sheet.addRow({ ...h, asset_class: CLASS_LABELS[h.asset_class] });

  const totals = sheet.addRow({
    symbol: 'TOTAL',
    invested: rows.reduce((a, h) => a + h.invested, 0),
    current_value: rows.reduce((a, h) => a + (h.current_value || 0), 0),
    unrealized_pnl: rows.reduce((a, h) => a + (h.unrealized_pnl || 0), 0),
    realized_pnl: rows.reduce((a, h) => a + h.realized_pnl, 0),
    dividends_received: rows.reduce((a, h) => a + h.dividends_received, 0),
  });
  totals.font = { bold: true };
  styleHeader(sheet);
  autoWidth(sheet);
  return sheet;
}

function addTransactionsSheet(wb, filters = {}) {
  let sql = `SELECT t.id, t.date, i.symbol, i.name, i.asset_class, t.type, t.quantity, t.price,
                    t.charges, t.source, t.notes
             FROM transactions t JOIN instruments i ON i.id = t.instrument_id WHERE 1=1`;
  const params = [];
  if (filters.asset_class) {
    sql += ' AND i.asset_class = ?';
    params.push(filters.asset_class);
  }
  if (filters.instrument_id) {
    sql += ' AND t.instrument_id = ?';
    params.push(filters.instrument_id);
  }
  sql += ' ORDER BY t.date DESC, t.id DESC';
  const rows = db.prepare(sql).all(...params);

  const sheet = wb.addWorksheet('Transactions');
  sheet.columns = [
    { header: 'Date', key: 'date' },
    { header: 'Symbol', key: 'symbol' },
    { header: 'Name', key: 'name' },
    { header: 'Asset Class', key: 'asset_class' },
    { header: 'Type', key: 'type' },
    { header: 'Source', key: 'source' },
    { header: 'Quantity', key: 'quantity', style: { numFmt: QTY } },
    { header: 'Price (₹)', key: 'price', style: { numFmt: MONEY } },
    { header: 'Charges (₹)', key: 'charges', style: { numFmt: MONEY } },
    { header: 'Amount (₹)', key: 'amount', style: { numFmt: MONEY } },
    { header: 'Notes', key: 'notes' },
  ];
  for (const r of rows)
    sheet.addRow({
      ...r,
      asset_class: CLASS_LABELS[r.asset_class],
      amount: r.quantity * r.price + (r.type === 'BUY' ? r.charges : -r.charges),
    });
  styleHeader(sheet);
  autoWidth(sheet);
  return sheet;
}

function addDividendsSheet(wb) {
  const rows = db
    .prepare(
      `SELECT d.date, i.symbol, i.name, i.asset_class, d.type, d.amount, d.notes
       FROM dividends d JOIN instruments i ON i.id = d.instrument_id
       ORDER BY d.date DESC`
    )
    .all();
  const sheet = wb.addWorksheet('Dividends & Income');
  sheet.columns = [
    { header: 'Date', key: 'date' },
    { header: 'Symbol', key: 'symbol' },
    { header: 'Name', key: 'name' },
    { header: 'Asset Class', key: 'asset_class' },
    { header: 'Income Type', key: 'type' },
    { header: 'Amount (₹)', key: 'amount', style: { numFmt: MONEY } },
    { header: 'Notes', key: 'notes' },
  ];
  for (const r of rows) sheet.addRow({ ...r, asset_class: CLASS_LABELS[r.asset_class] });
  const totals = sheet.addRow({ symbol: 'TOTAL', amount: rows.reduce((a, r) => a + r.amount, 0) });
  totals.font = { bold: true };
  styleHeader(sheet);
  autoWidth(sheet);
  return sheet;
}

function addSipsSheet(wb) {
  const rows = db
    .prepare(
      `SELECT s.id, i.symbol, i.name, s.amount, s.day_of_month, s.start_date, s.end_date, s.active,
              COUNT(t.id) AS installments, COALESCE(SUM(t.quantity * t.price), 0) AS invested,
              COALESCE(SUM(t.quantity), 0) AS units
       FROM sips s
       JOIN instruments i ON i.id = s.instrument_id
       LEFT JOIN transactions t ON t.sip_id = s.id
       GROUP BY s.id ORDER BY i.symbol`
    )
    .all();
  const sheet = wb.addWorksheet('SIPs');
  sheet.columns = [
    { header: 'Fund', key: 'symbol' },
    { header: 'Name', key: 'name' },
    { header: 'Monthly Amount (₹)', key: 'amount', style: { numFmt: MONEY } },
    { header: 'SIP Day', key: 'day_of_month' },
    { header: 'Start Date', key: 'start_date' },
    { header: 'Status', key: 'status' },
    { header: 'Installments', key: 'installments' },
    { header: 'Units Accumulated', key: 'units', style: { numFmt: QTY } },
    { header: 'Total Invested (₹)', key: 'invested', style: { numFmt: MONEY } },
  ];
  for (const r of rows) sheet.addRow({ ...r, status: r.active ? 'Active' : 'Stopped' });
  styleHeader(sheet);
  autoWidth(sheet);
  return sheet;
}

function addSummarySheet(wb) {
  const s = summary();
  const div = dividendSummary();
  const sheet = wb.addWorksheet('Summary');
  sheet.columns = [
    { header: 'Asset Class', key: 'asset_class' },
    { header: 'Holdings', key: 'holdings_count' },
    { header: 'Invested (₹)', key: 'invested', style: { numFmt: MONEY } },
    { header: 'Current Value (₹)', key: 'current_value', style: { numFmt: MONEY } },
    { header: 'Unrealized P&L (₹)', key: 'unrealized_pnl', style: { numFmt: MONEY } },
    { header: 'P&L %', key: 'pnl_pct', style: { numFmt: '0.00' } },
    { header: 'Realized P&L (₹)', key: 'realized_pnl', style: { numFmt: MONEY } },
    { header: 'Income Received (₹)', key: 'dividends_received', style: { numFmt: MONEY } },
  ];
  for (const row of s.by_asset_class) sheet.addRow({ ...row, asset_class: CLASS_LABELS[row.asset_class] });
  const totals = sheet.addRow({ asset_class: 'TOTAL', ...s.total });
  totals.font = { bold: true };
  sheet.addRow({});
  sheet.addRow({ asset_class: 'Total dividends & income received', invested: div.total }).font = { italic: true };
  styleHeader(sheet);
  autoWidth(sheet);
  return sheet;
}

async function buildWorkbook(kind, filters = {}) {
  const wb = new ExcelJS.Workbook();
  wb.creator = 'Investment Portfolio Tracker';
  wb.created = new Date();

  switch (kind) {
    case 'holdings':
      addHoldingsSheet(wb, filters.asset_class || null);
      break;
    case 'transactions':
      addTransactionsSheet(wb, filters);
      break;
    case 'dividends':
      addDividendsSheet(wb);
      break;
    case 'sips':
      addSipsSheet(wb);
      break;
    case 'portfolio':
      addSummarySheet(wb);
      for (const ac of ['EQUITY', 'MUTUAL_FUND', 'GOV_BOND', 'GOLD_BOND']) addHoldingsSheet(wb, ac);
      addSipsSheet(wb);
      addDividendsSheet(wb);
      addTransactionsSheet(wb);
      break;
    default:
      throw new Error(`Unknown export kind: ${kind}`);
  }
  return wb;
}

module.exports = { buildWorkbook };

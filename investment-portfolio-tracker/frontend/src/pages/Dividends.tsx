import { useCallback, useEffect, useState } from 'react'
import type { Dividend, DividendSummary, Instrument } from '../lib/api'
import { CLASS_META, INCOME_META, api, fmtCompact, fmtINR2 } from '../lib/api'
import { MonthlyIncomeChart } from '../components/charts'
import { ExportButton, PageHead, StatTile } from '../components/ui'

export default function Dividends() {
  const [summary, setSummary] = useState<DividendSummary | null>(null)
  const [records, setRecords] = useState<Dividend[]>([])
  const [instruments, setInstruments] = useState<Instrument[]>([])
  const [classFilter, setClassFilter] = useState<string>('')
  const [form, setForm] = useState({
    instrument_id: '',
    date: new Date().toISOString().slice(0, 10),
    type: 'DIVIDEND',
    amount: '',
    notes: '',
  })

  const reload = useCallback(() => {
    api<DividendSummary>('/dividends/summary').then(setSummary)
    api<Dividend[]>(`/dividends${classFilter ? `?asset_class=${classFilter}` : ''}`).then(setRecords)
    api<Instrument[]>('/instruments').then(setInstruments)
  }, [classFilter])
  useEffect(reload, [reload])

  const addRecord = async () => {
    await api('/dividends', {
      method: 'POST',
      body: JSON.stringify({
        instrument_id: Number(form.instrument_id),
        date: form.date,
        type: form.type,
        amount: Number(form.amount),
        notes: form.notes || null,
      }),
    })
    setForm({ ...form, amount: '', notes: '' })
    reload()
  }

  if (!summary) return <div className="empty-note">Loading…</div>

  const thisFY = summary.by_fiscal_year.at(-1)
  const last12 = summary.by_month
    .filter((r) => r.month >= new Date(Date.now() - 365 * 864e5).toISOString().slice(0, 7))
    .reduce((a, r) => a + r.amount, 0)

  return (
    <>
      <PageHead
        title="Dividends & Income"
        sub="Equity dividends, mutual fund payouts, bond coupons and SGB interest"
        actions={<ExportButton href="/api/export/dividends" label="Export to Excel" />}
      />

      <div className="grid kpi-row" style={{ marginBottom: 14 }}>
        <StatTile label="Total income received" value={fmtCompact(summary.total)} />
        <StatTile label={`This fiscal year${thisFY ? ` (${thisFY.fiscal_year})` : ''}`} value={fmtCompact(thisFY?.amount ?? 0)} />
        <StatTile label="Last 12 months" value={fmtCompact(last12)} />
        <StatTile label="Income sources" value={String(new Set(summary.by_instrument.map((r) => r.symbol)).size)} />
      </div>

      <div className="grid" style={{ gridTemplateColumns: '2fr 1fr', marginBottom: 14 }}>
        <div className="card">
          <h2>Monthly income</h2>
          <p className="caption">Stacked by income type</p>
          <MonthlyIncomeChart summary={summary} />
        </div>
        <div className="card">
          <h2>By fiscal year</h2>
          <p className="caption">April–March, cumulative income</p>
          <div className="table-wrap">
            <table>
              <thead>
                <tr>
                  <th>Fiscal year</th>
                  <th className="num">Income</th>
                </tr>
              </thead>
              <tbody>
                {summary.by_fiscal_year.map((r) => (
                  <tr key={r.fiscal_year}>
                    <td>{r.fiscal_year}</td>
                    <td className="num">{fmtINR2(r.amount)}</td>
                  </tr>
                ))}
                <tr className="total-row">
                  <td>Total</td>
                  <td className="num">{fmtINR2(summary.total)}</td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>
      </div>

      <div className="grid charts-2" style={{ marginBottom: 14 }}>
        <div className="card">
          <h2>Top income sources</h2>
          <p className="caption">Cumulative income per instrument</p>
          <div className="table-wrap">
            <table>
              <thead>
                <tr>
                  <th>Instrument</th>
                  <th>Type</th>
                  <th className="num">Payouts</th>
                  <th className="num">Total</th>
                </tr>
              </thead>
              <tbody>
                {summary.by_instrument.slice(0, 10).map((r, i) => (
                  <tr key={i}>
                    <td>
                      <div className="sym">{r.symbol}</div>
                      <div className="name-sub">{r.name}</div>
                    </td>
                    <td>
                      <span
                        className="badge"
                        style={{ background: `color-mix(in srgb, ${INCOME_META[r.type].cssVar} 14%, transparent)` }}
                      >
                        {INCOME_META[r.type].label}
                      </span>
                    </td>
                    <td className="num">{r.payouts}</td>
                    <td className="num">{fmtINR2(r.amount)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        <div className="card">
          <h2>Record income</h2>
          <p className="caption">Log a dividend, coupon, IDCW payout or SGB interest credit</p>
          <div className="form-grid">
            <label className="field">
              Instrument
              <select value={form.instrument_id} onChange={(e) => setForm({ ...form, instrument_id: e.target.value })}>
                <option value="">Select…</option>
                {instruments.map((i) => (
                  <option key={i.id} value={i.id}>
                    {i.symbol} — {i.name}
                  </option>
                ))}
              </select>
            </label>
            <label className="field">
              Date
              <input type="date" value={form.date} onChange={(e) => setForm({ ...form, date: e.target.value })} />
            </label>
            <label className="field">
              Type
              <select value={form.type} onChange={(e) => setForm({ ...form, type: e.target.value })}>
                {Object.entries(INCOME_META).map(([k, v]) => (
                  <option key={k} value={k}>
                    {v.label}
                  </option>
                ))}
              </select>
            </label>
            <label className="field">
              Amount (₹)
              <input type="number" min="0" step="any" value={form.amount} onChange={(e) => setForm({ ...form, amount: e.target.value })} />
            </label>
            <label className="field">
              Notes
              <input value={form.notes} onChange={(e) => setForm({ ...form, notes: e.target.value })} />
            </label>
            <button className="btn primary" disabled={!form.instrument_id || !Number(form.amount)} onClick={addRecord}>
              Save
            </button>
          </div>
        </div>
      </div>

      <div className="card">
        <h2>All income records</h2>
        <div className="filter-row" style={{ marginTop: 8 }}>
          <button className={`chip ${classFilter === '' ? 'active' : ''}`} onClick={() => setClassFilter('')}>
            All
          </button>
          {(['EQUITY', 'MUTUAL_FUND', 'GOV_BOND', 'GOLD_BOND'] as const).map((ac) => (
            <button
              key={ac}
              className={`chip ${classFilter === ac ? 'active' : ''}`}
              onClick={() => setClassFilter(ac)}
            >
              {CLASS_META[ac].label}
            </button>
          ))}
        </div>
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Date</th>
                <th>Instrument</th>
                <th>Type</th>
                <th className="num">Amount</th>
                <th>Notes</th>
              </tr>
            </thead>
            <tbody>
              {records.map((d) => (
                <tr key={d.id}>
                  <td>{d.date}</td>
                  <td>
                    <div className="sym">{d.symbol}</div>
                    <div className="name-sub">{d.name}</div>
                  </td>
                  <td>
                    <span
                      className="badge"
                      style={{ background: `color-mix(in srgb, ${INCOME_META[d.type].cssVar} 14%, transparent)` }}
                    >
                      {INCOME_META[d.type].label}
                    </span>
                  </td>
                  <td className="num">{fmtINR2(d.amount)}</td>
                  <td className="name-sub">{d.notes}</td>
                </tr>
              ))}
              {!records.length && (
                <tr>
                  <td colSpan={5} className="empty-note">
                    No income records for this filter.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>
    </>
  )
}

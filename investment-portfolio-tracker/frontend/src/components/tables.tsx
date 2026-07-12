import { useState } from 'react'
import type { AssetClass, Holding, Instrument } from '../lib/api'
import { CLASS_META, api, fmtINR2, fmtCompact, fmtQty } from '../lib/api'
import { PnlText } from './ui'

export function HoldingsTable({
  holdings,
  showBondCols,
  showClass,
  onSelect,
  selectedId,
}: {
  holdings: Holding[]
  showBondCols?: boolean
  showClass?: boolean
  onSelect?: (h: Holding) => void
  selectedId?: number
}) {
  const rows = holdings.filter((h) => h.quantity > 0 || h.realized_pnl !== 0)
  if (!rows.length) return <div className="empty-note">No holdings yet — record a purchase below.</div>
  const sum = (f: (h: Holding) => number) => rows.reduce((a, h) => a + f(h), 0)
  return (
    <div className="table-wrap">
      <table>
        <thead>
          <tr>
            <th>Instrument</th>
            {showClass && <th>Class</th>}
            {showBondCols && <th className="num">Coupon</th>}
            {showBondCols && <th>Maturity</th>}
            <th className="num">Qty</th>
            <th className="num">Avg cost</th>
            <th className="num">Invested</th>
            <th className="num">Price</th>
            <th className="num">Value</th>
            <th className="num">Unrealized P&L</th>
            <th className="num">Realized P&L</th>
            <th className="num">Income</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((h) => (
            <tr
              key={h.instrument_id}
              onClick={() => onSelect?.(h)}
              style={
                onSelect
                  ? {
                      cursor: 'pointer',
                      background:
                        selectedId === h.instrument_id
                          ? 'color-mix(in srgb, var(--accent) 8%, transparent)'
                          : undefined,
                    }
                  : undefined
              }
            >
              <td>
                <div className="sym">{h.symbol}</div>
                <div className="name-sub">{h.name}</div>
              </td>
              {showClass && (
                <td>
                  <span
                    className="badge"
                    style={{
                      background: `color-mix(in srgb, ${CLASS_META[h.asset_class].cssVar} 14%, transparent)`,
                    }}
                  >
                    {CLASS_META[h.asset_class].label}
                  </span>
                </td>
              )}
              {showBondCols && <td className="num">{h.coupon_rate != null ? `${h.coupon_rate}%` : '—'}</td>}
              {showBondCols && <td>{h.maturity_date ?? '—'}</td>}
              <td className="num">{fmtQty(h.quantity)}</td>
              <td className="num">{fmtINR2(h.avg_cost)}</td>
              <td className="num">{fmtCompact(h.invested)}</td>
              <td className="num">{fmtINR2(h.current_price)}</td>
              <td className="num">{fmtCompact(h.current_value)}</td>
              <td className="num">
                <PnlText value={h.unrealized_pnl} pct={h.unrealized_pnl_pct} />
              </td>
              <td className="num">{h.realized_pnl ? <PnlText value={h.realized_pnl} /> : '—'}</td>
              <td className="num">{h.dividends_received ? fmtCompact(h.dividends_received) : '—'}</td>
            </tr>
          ))}
          <tr className="total-row">
            <td>Total</td>
            {showClass && <td />}
            {showBondCols && <td />}
            {showBondCols && <td />}
            <td />
            <td />
            <td className="num">{fmtCompact(sum((h) => h.invested))}</td>
            <td />
            <td className="num">{fmtCompact(sum((h) => h.current_value ?? 0))}</td>
            <td className="num">
              <PnlText value={sum((h) => h.unrealized_pnl ?? 0)} />
            </td>
            <td className="num">
              <PnlText value={sum((h) => h.realized_pnl)} />
            </td>
            <td className="num">{fmtCompact(sum((h) => h.dividends_received))}</td>
          </tr>
        </tbody>
      </table>
    </div>
  )
}

export function AddTransactionForm({
  instruments,
  assetClass,
  onSaved,
}: {
  instruments: Instrument[]
  assetClass?: AssetClass
  onSaved: () => void
}) {
  const list = assetClass ? instruments.filter((i) => i.asset_class === assetClass) : instruments
  const [form, setForm] = useState({
    instrument_id: '',
    date: new Date().toISOString().slice(0, 10),
    type: 'BUY',
    quantity: '',
    price: '',
    charges: '0',
  })
  const [error, setError] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)

  const submit = async () => {
    setError(null)
    setBusy(true)
    try {
      await api('/transactions', {
        method: 'POST',
        body: JSON.stringify({
          instrument_id: Number(form.instrument_id),
          date: form.date,
          type: form.type,
          quantity: Number(form.quantity),
          price: Number(form.price),
          charges: Number(form.charges) || 0,
        }),
      })
      setForm({ ...form, quantity: '', price: '' })
      onSaved()
    } catch (e) {
      setError((e as Error).message)
    } finally {
      setBusy(false)
    }
  }

  return (
    <div className="card">
      <h2>Record a transaction</h2>
      <p className="caption">Buys and sells update holdings, averages and valuations immediately.</p>
      <div className="form-grid">
        <label className="field">
          Instrument
          <select
            value={form.instrument_id}
            onChange={(e) => setForm({ ...form, instrument_id: e.target.value })}
          >
            <option value="">Select…</option>
            {list.map((i) => (
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
            <option value="BUY">Buy</option>
            <option value="SELL">Sell</option>
          </select>
        </label>
        <label className="field">
          Quantity / Units
          <input
            type="number"
            min="0"
            step="any"
            value={form.quantity}
            onChange={(e) => setForm({ ...form, quantity: e.target.value })}
          />
        </label>
        <label className="field">
          Price / NAV (₹)
          <input
            type="number"
            min="0"
            step="any"
            value={form.price}
            onChange={(e) => setForm({ ...form, price: e.target.value })}
          />
        </label>
        <label className="field">
          Charges (₹)
          <input
            type="number"
            min="0"
            step="any"
            value={form.charges}
            onChange={(e) => setForm({ ...form, charges: e.target.value })}
          />
        </label>
        <button
          className="btn primary"
          disabled={busy || !form.instrument_id || !form.quantity || !form.price}
          onClick={submit}
        >
          {busy ? 'Saving…' : 'Save'}
        </button>
      </div>
      {error && <div style={{ color: 'var(--down)', marginTop: 8, fontSize: 13 }}>{error}</div>}
    </div>
  )
}

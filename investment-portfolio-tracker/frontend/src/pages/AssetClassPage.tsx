import { useCallback, useEffect, useState } from 'react'
import type { AssetClass, Holding, Instrument, Sip } from '../lib/api'
import { api, fmtCompact, fmtINR2, fmtQty } from '../lib/api'
import { PriceHistoryChart } from '../components/charts'
import { AddTransactionForm, HoldingsTable } from '../components/tables'
import { ExportButton, PageHead, StatTile } from '../components/ui'

export default function AssetClassPage({
  assetClass,
  title,
  sub,
}: {
  assetClass: AssetClass
  title: string
  sub: string
}) {
  const [holdings, setHoldings] = useState<Holding[]>([])
  const [instruments, setInstruments] = useState<Instrument[]>([])
  const [selected, setSelected] = useState<Holding | null>(null)
  const [prices, setPrices] = useState<{ date: string; price: number }[]>([])

  const reload = useCallback(() => {
    api<Holding[]>(`/holdings?asset_class=${assetClass}`).then((hs) => {
      setHoldings(hs)
      setSelected((cur) => hs.find((h) => h.instrument_id === cur?.instrument_id) ?? hs.find((h) => h.quantity > 0) ?? null)
    })
    api<Instrument[]>(`/instruments?asset_class=${assetClass}`).then(setInstruments)
  }, [assetClass])

  useEffect(reload, [reload])
  useEffect(() => {
    if (selected) api<{ date: string; price: number }[]>(`/prices/${selected.instrument_id}`).then(setPrices)
  }, [selected?.instrument_id])

  const open = holdings.filter((h) => h.quantity > 0)
  const invested = open.reduce((a, h) => a + h.invested, 0)
  const value = open.reduce((a, h) => a + (h.current_value ?? 0), 0)
  const income = holdings.reduce((a, h) => a + h.dividends_received, 0)
  const isBond = assetClass === 'GOV_BOND' || assetClass === 'GOLD_BOND'

  return (
    <>
      <PageHead
        title={title}
        sub={sub}
        actions={
          <>
            <ExportButton href={`/api/export/holdings?asset_class=${assetClass}`} label="Export holdings" />
            <ExportButton href={`/api/export/transactions?asset_class=${assetClass}`} label="Export transactions" />
          </>
        }
      />

      <div className="grid kpi-row" style={{ marginBottom: 14 }}>
        <StatTile label="Current value" value={fmtCompact(value)} delta={value - invested} deltaLabel="unrealized" />
        <StatTile label="Invested" value={fmtCompact(invested)} />
        <StatTile label="Open positions" value={String(open.length)} />
        <StatTile
          label={isBond ? 'Interest received' : 'Dividends / payouts received'}
          value={fmtCompact(income)}
        />
      </div>

      <div className="card" style={{ marginBottom: 14 }}>
        <h2>Holdings</h2>
        <p className="caption">Click a row to see its price history</p>
        <HoldingsTable
          holdings={holdings}
          showBondCols={isBond}
          onSelect={setSelected}
          selectedId={selected?.instrument_id}
        />
      </div>

      {selected && (
        <div className="card" style={{ marginBottom: 14 }}>
          <h2>
            {selected.symbol} — {isBond ? 'price' : assetClass === 'MUTUAL_FUND' ? 'NAV' : 'price'} history
          </h2>
          <p className="caption">{selected.name}</p>
          <PriceHistoryChart data={prices} assetClass={assetClass} />
        </div>
      )}

      {assetClass === 'MUTUAL_FUND' && <SipSection onChanged={reload} />}

      <AddTransactionForm instruments={instruments} assetClass={assetClass} onSaved={reload} />
    </>
  )
}

function SipSection({ onChanged }: { onChanged: () => void }) {
  const [sips, setSips] = useState<Sip[]>([])
  const [instruments, setInstruments] = useState<Instrument[]>([])
  const [form, setForm] = useState({ instrument_id: '', amount: '', day_of_month: '5', start_date: new Date().toISOString().slice(0, 10) })
  const [exec, setExec] = useState<{ sipId: number; date: string; nav: string } | null>(null)

  const reload = useCallback(() => {
    api<Sip[]>('/sips').then(setSips)
    api<Instrument[]>('/instruments?asset_class=MUTUAL_FUND').then(setInstruments)
  }, [])
  useEffect(reload, [reload])

  const addSip = async () => {
    await api('/sips', {
      method: 'POST',
      body: JSON.stringify({
        instrument_id: Number(form.instrument_id),
        amount: Number(form.amount),
        day_of_month: Number(form.day_of_month),
        start_date: form.start_date,
      }),
    })
    setForm({ ...form, instrument_id: '', amount: '' })
    reload()
  }

  const toggle = async (s: Sip) => {
    await api(`/sips/${s.id}`, { method: 'PATCH', body: JSON.stringify({ active: !s.active }) })
    reload()
  }

  const runInstallment = async () => {
    if (!exec) return
    await api(`/sips/${exec.sipId}/execute`, {
      method: 'POST',
      body: JSON.stringify({ date: exec.date, nav: Number(exec.nav) }),
    })
    setExec(null)
    reload()
    onChanged()
  }

  return (
    <div className="card" style={{ marginBottom: 14 }}>
      <h2>Systematic Investment Plans (SIP)</h2>
      <p className="caption">Recurring monthly purchases; each installment is recorded as a BUY at that day's NAV</p>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Fund</th>
              <th className="num">Monthly amount</th>
              <th className="num">SIP day</th>
              <th>Started</th>
              <th className="num">Installments</th>
              <th className="num">Units</th>
              <th className="num">Invested</th>
              <th>Status</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {sips.map((s) => (
              <tr key={s.id}>
                <td>
                  <div className="sym">{s.symbol}</div>
                  <div className="name-sub">{s.name}</div>
                </td>
                <td className="num">{fmtINR2(s.amount)}</td>
                <td className="num">{s.day_of_month}</td>
                <td>{s.start_date}</td>
                <td className="num">{s.installments}</td>
                <td className="num">{fmtQty(s.units)}</td>
                <td className="num">{fmtCompact(s.invested)}</td>
                <td>
                  <span
                    className="badge"
                    style={{
                      background: s.active
                        ? 'color-mix(in srgb, var(--up) 15%, transparent)'
                        : 'color-mix(in srgb, var(--text-muted) 15%, transparent)',
                    }}
                  >
                    {s.active ? '● Active' : '○ Paused'}
                  </span>
                </td>
                <td style={{ display: 'flex', gap: 6 }}>
                  <button className="btn" onClick={() => setExec({ sipId: s.id, date: new Date().toISOString().slice(0, 10), nav: '' })}>
                    Record installment
                  </button>
                  <button className="btn" onClick={() => toggle(s)}>
                    {s.active ? 'Pause' : 'Resume'}
                  </button>
                </td>
              </tr>
            ))}
            {!sips.length && (
              <tr>
                <td colSpan={9} className="empty-note">
                  No SIPs yet — start one below.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>

      {exec && (
        <div className="form-grid" style={{ marginTop: 12 }}>
          <label className="field">
            Installment date
            <input type="date" value={exec.date} onChange={(e) => setExec({ ...exec, date: e.target.value })} />
          </label>
          <label className="field">
            NAV (₹)
            <input type="number" min="0" step="any" value={exec.nav} onChange={(e) => setExec({ ...exec, nav: e.target.value })} />
          </label>
          <button className="btn primary" disabled={!Number(exec.nav)} onClick={runInstallment}>
            Save installment
          </button>
          <button className="btn" onClick={() => setExec(null)}>
            Cancel
          </button>
        </div>
      )}

      <div className="form-grid" style={{ marginTop: 14 }}>
        <label className="field">
          Fund
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
          Monthly amount (₹)
          <input type="number" min="0" value={form.amount} onChange={(e) => setForm({ ...form, amount: e.target.value })} />
        </label>
        <label className="field">
          SIP day of month
          <input type="number" min="1" max="28" value={form.day_of_month} onChange={(e) => setForm({ ...form, day_of_month: e.target.value })} />
        </label>
        <label className="field">
          Start date
          <input type="date" value={form.start_date} onChange={(e) => setForm({ ...form, start_date: e.target.value })} />
        </label>
        <button className="btn primary" disabled={!form.instrument_id || !Number(form.amount)} onClick={addSip}>
          Start SIP
        </button>
      </div>
    </div>
  )
}

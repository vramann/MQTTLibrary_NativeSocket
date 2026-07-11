import { useEffect, useState } from 'react'
import { Link } from 'react-router-dom'
import type { HistoryPoint, Holding, Summary } from '../lib/api'
import { CLASS_META, api, fmtCompact, fmtPct } from '../lib/api'
import { AllocationDonut, ClassPnlBars, ValueTrendChart } from '../components/charts'
import { HoldingsTable } from '../components/tables'
import { ExportButton, PageHead, PnlText, StatTile } from '../components/ui'

const RANGES = [
  { label: '1Y', months: 12 },
  { label: '2Y', months: 24 },
  { label: '3Y', months: 36 },
  { label: '5Y', months: 60 },
]

const CLASS_ROUTES: Record<string, string> = {
  EQUITY: '/equity',
  MUTUAL_FUND: '/mutual-funds',
  GOV_BOND: '/gov-bonds',
  GOLD_BOND: '/gold-bonds',
}

export default function Dashboard() {
  const [summary, setSummary] = useState<Summary | null>(null)
  const [history, setHistory] = useState<HistoryPoint[]>([])
  const [holdings, setHoldings] = useState<Holding[]>([])
  const [months, setMonths] = useState(36)

  useEffect(() => {
    api<Summary>('/summary').then(setSummary)
    api<Holding[]>('/holdings').then(setHoldings)
  }, [])
  useEffect(() => {
    api<HistoryPoint[]>(`/history?months=${months}`).then(setHistory)
  }, [months])

  if (!summary) return <div className="empty-note">Loading…</div>
  const t = summary.total

  return (
    <>
      <PageHead
        title="Dashboard"
        sub="All investments at a glance"
        actions={<ExportButton href="/api/export/portfolio" label="Export full portfolio" />}
      />

      <div className="grid kpi-row" style={{ marginBottom: 14 }}>
        <StatTile label="Current value" value={fmtCompact(t.current_value)} delta={t.unrealized_pnl} deltaLabel={`(${fmtPct(t.pnl_pct)}) unrealized`} />
        <StatTile label="Amount invested" value={fmtCompact(t.invested)} />
        <StatTile label="Realized P&L" value={fmtCompact(t.realized_pnl)} />
        <StatTile label="Dividends & income received" value={fmtCompact(t.dividends_received)} />
      </div>

      <div className="filter-row">
        {RANGES.map((r) => (
          <button
            key={r.label}
            className={`chip ${months === r.months ? 'active' : ''}`}
            onClick={() => setMonths(r.months)}
          >
            {r.label}
          </button>
        ))}
      </div>

      <div className="grid" style={{ gridTemplateColumns: '2fr 1fr', marginBottom: 14 }}>
        <div className="card">
          <h2>Portfolio growth</h2>
          <p className="caption">Market value vs. cumulative amount invested</p>
          <ValueTrendChart data={history} />
        </div>
        <div className="card">
          <h2>Allocation</h2>
          <p className="caption">Current value by asset class</p>
          <AllocationDonut classes={summary.by_asset_class} />
        </div>
      </div>

      <div className="grid charts-2" style={{ marginBottom: 14 }}>
        <div className="card">
          <h2>Unrealized P&L by asset class</h2>
          <p className="caption">Gain or loss on open positions</p>
          <ClassPnlBars classes={summary.by_asset_class} />
        </div>
        <div className="card">
          <h2>Asset class summary</h2>
          <p className="caption">Click a class to open its page</p>
          <div className="table-wrap">
            <table>
              <thead>
                <tr>
                  <th>Asset class</th>
                  <th className="num">Invested</th>
                  <th className="num">Value</th>
                  <th className="num">P&L</th>
                  <th className="num">Income</th>
                </tr>
              </thead>
              <tbody>
                {summary.by_asset_class.map((c) => (
                  <tr key={c.asset_class}>
                    <td>
                      <Link
                        to={CLASS_ROUTES[c.asset_class] ?? '/'}
                        style={{ color: 'inherit', fontWeight: 600, display: 'inline-flex', alignItems: 'center', gap: 7 }}
                      >
                        <span className="nav-dot" style={{ background: CLASS_META[c.asset_class].cssVar }} />
                        {CLASS_META[c.asset_class].label}
                      </Link>
                    </td>
                    <td className="num">{fmtCompact(c.invested)}</td>
                    <td className="num">{fmtCompact(c.current_value)}</td>
                    <td className="num">
                      <PnlText value={c.unrealized_pnl} pct={c.pnl_pct} />
                    </td>
                    <td className="num">{fmtCompact(c.dividends_received)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      </div>

      <div className="card">
        <h2>All holdings</h2>
        <p className="caption">Every open position across asset classes</p>
        <HoldingsTable holdings={holdings} showClass />
      </div>
    </>
  )
}

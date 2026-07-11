import { useCallback, useEffect, useState } from 'react'
import type { Instrument, Transaction } from '../lib/api'
import { CLASS_META, api, fmtINR2, fmtCompact, fmtQty } from '../lib/api'
import { AddTransactionForm } from '../components/tables'
import { ExportButton, PageHead } from '../components/ui'

export default function Transactions() {
  const [records, setRecords] = useState<Transaction[]>([])
  const [instruments, setInstruments] = useState<Instrument[]>([])
  const [classFilter, setClassFilter] = useState<string>('')

  const reload = useCallback(() => {
    api<Transaction[]>(`/transactions${classFilter ? `?asset_class=${classFilter}` : ''}`).then(setRecords)
    api<Instrument[]>('/instruments').then(setInstruments)
  }, [classFilter])
  useEffect(reload, [reload])

  const remove = async (id: number) => {
    if (!confirm('Delete this transaction? Holdings will be recomputed.')) return
    await api(`/transactions/${id}`, { method: 'DELETE' })
    reload()
  }

  return (
    <>
      <PageHead
        title="Transactions"
        sub="Every buy and sell, including SIP installments"
        actions={
          <ExportButton
            href={`/api/export/transactions${classFilter ? `?asset_class=${classFilter}` : ''}`}
            label="Export to Excel"
          />
        }
      />

      <div className="filter-row">
        <button className={`chip ${classFilter === '' ? 'active' : ''}`} onClick={() => setClassFilter('')}>
          All
        </button>
        {(['EQUITY', 'MUTUAL_FUND', 'GOV_BOND', 'GOLD_BOND'] as const).map((ac) => (
          <button key={ac} className={`chip ${classFilter === ac ? 'active' : ''}`} onClick={() => setClassFilter(ac)}>
            {CLASS_META[ac].label}
          </button>
        ))}
      </div>

      <div className="card" style={{ marginBottom: 14 }}>
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Date</th>
                <th>Instrument</th>
                <th>Class</th>
                <th>Type</th>
                <th>Source</th>
                <th className="num">Qty</th>
                <th className="num">Price</th>
                <th className="num">Charges</th>
                <th className="num">Amount</th>
                <th></th>
              </tr>
            </thead>
            <tbody>
              {records.map((t) => (
                <tr key={t.id}>
                  <td>{t.date}</td>
                  <td>
                    <div className="sym">{t.symbol}</div>
                    <div className="name-sub">{t.name}</div>
                  </td>
                  <td>
                    <span
                      className="badge"
                      style={{
                        background: `color-mix(in srgb, ${CLASS_META[t.asset_class].cssVar} 14%, transparent)`,
                      }}
                    >
                      {CLASS_META[t.asset_class].label}
                    </span>
                  </td>
                  <td>
                    <span className={t.type === 'BUY' ? 'delta-up' : 'delta-down'} style={{ fontWeight: 600 }}>
                      {t.type}
                    </span>
                  </td>
                  <td>{t.source === 'SIP' ? 'SIP' : 'Lumpsum'}</td>
                  <td className="num">{fmtQty(t.quantity)}</td>
                  <td className="num">{fmtINR2(t.price)}</td>
                  <td className="num">{t.charges ? fmtINR2(t.charges) : '—'}</td>
                  <td className="num">{fmtCompact(t.quantity * t.price)}</td>
                  <td>
                    <button className="btn" onClick={() => remove(t.id)}>
                      ✕
                    </button>
                  </td>
                </tr>
              ))}
              {!records.length && (
                <tr>
                  <td colSpan={10} className="empty-note">
                    No transactions for this filter.
                  </td>
                </tr>
              )}
            </tbody>
          </table>
        </div>
      </div>

      <AddTransactionForm instruments={instruments} onSaved={reload} />
    </>
  )
}

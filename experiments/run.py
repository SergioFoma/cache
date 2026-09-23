#!/usr/bin/env python3
"""Generate fixed workloads, run the repository's C++ caches, and draw lab figures."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import subprocess
import tempfile
from collections import Counter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'report'
NAMES = ['LRU', 'LFU', 'ARC', '2Q', 'LIRS', 'BELADY']
TITLES = {
 'uniform': 'Равномерное распределение', 'zipf': 'Распределение Ципфа',
 'normal': 'Дискретизированное нормальное', 'scan': 'Однократный проход',
 'lru': 'Смена повторяемых блоков', 'lfu': 'Популярное ядро и сканирование',
 'arc': 'Редкие обращения к сменяемому ядру', 'twoq': 'Частые обращения к сменяемому ядру',
 'lirs': 'Циклический блок больше кеша', 'belady': 'Цикл из 101 ключа'}


def dataset(name, seed):
    r = random.Random(seed)
    if name == 'uniform': return [r.randrange(400) for _ in range(20000)]
    if name == 'zipf': return r.choices(range(400), weights=[1/(k+1)**1.2 for k in range(400)], k=20000)
    if name == 'normal':
        x = []
        while len(x) < 20000:
            key = round(r.gauss(199.5, 60))
            if 0 <= key < 400: x.append(key)
        return x
    if name == 'scan': return list(range(20000))
    if name == 'belady': return list(range(101))*200
    if name in ('lru', 'lirs'):
        hot, block, repeat = (48, 100, 5) if name == 'lru' else (18, 103, 7)
        x = list(range(hot))*4
        for cycle in range(40):
            x += [r.randrange(hot) for _ in range(hot*3)]
            x += list(range(1000+cycle*block, 1000+(cycle+1)*block))*repeat
        return x
    if name == 'lfu':
        hot, scan = 89, 54
        x = list(range(hot))*4
        for cycle in range(30):
            x += [r.randrange(hot) for _ in range(hot*5)]
            x += [1000+cycle*scan+j for j in range(scan) for _ in range(2)]
        return x
    if name in ('arc', 'twoq'):
        hot, prob, phase = (53, .23, 3100) if name == 'arc' else (45, .4, 4500)
        x = list(range(100))*10
        for cycle in range(4):
            for t in range(phase):
                if r.random() < prob: x.append(1000+cycle*hot+r.randrange(hot))
                else: x.extend([100000+cycle*phase+t]*2)
        return x
    raise ValueError(name)


def encode(trace, capacity):
    return f'{capacity} {len(trace)}\n'+' '.join(map(str, trace))+'\n'


def batch(binary, traces, args=()):
    result = subprocess.run([str(binary), *map(str, args)],
        input=''.join(encode(x, c) for x, c in traces), text=True, capture_output=True, check=True)
    rows = [list(map(int, line.split(','))) for line in result.stdout.splitlines()]
    assert len(rows) == len(traces)
    return rows


def write_csv(name, rows):
    with (OUT/'data'/name).open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='clang++')
    parser.add_argument('--json-include', type=Path, default=ROOT/'build/_deps/json-src/single_include')
    args = parser.parse_args()
    for folder in ('data', 'figures', 'configs'): (OUT/folder).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cache-lab-') as temp:
        temp = Path(temp)
        flags = ['-std=c++20', '-O2', '-Wall', '-Wextra', '-I', str(ROOT/'include')]
        subprocess.run([args.cxx, *flags, str(ROOT/'experiments/benchmark.cpp'), '-o', str(temp/'benchmark')], check=True)
        subprocess.run([args.cxx, *flags, '-I', str(args.json_include), str(ROOT/'experiments/hierarchy.cpp'),
                        str(ROOT/'src/parser.cpp'), '-o', str(temp/'hierarchy')], check=True)
        traces = {name: dataset(name, 0) for name in TITLES}
        jobs = [(name, seed, dataset(name, seed)) for name in TITLES for seed in range(10)]
        measured = batch(temp/'benchmark', [(x, 100) for _, _, x in jobs])
        rows = []
        for (name, seed, x), hits in zip(jobs, measured):
            assert all(0 <= h <= hits[-1] <= len(x) for h in hits)
            for algorithm, h in zip(NAMES, hits):
                rows.append(dict(dataset=name, seed=seed, capacity=100, requests=len(x), algorithm=algorithm,
                                 hits=h, misses=len(x)-h, hit_rate=h/len(x)))
        write_csv('results.csv', rows)
        primary = {name: measured[i*10] for i, name in enumerate(TITLES)}
        for name, expected in [('lru',0), ('lfu',1), ('arc',2), ('twoq',3), ('lirs',4), ('belady',5)]:
            h = primary[name]
            rivals = range(6) if expected == 5 else range(5)
            assert all(h[expected] > h[j] for j in rivals if j != expected), (name, h)
        assert primary['scan'] == [0]*6
        # Independent limiting cases: all repeated keys fit; scan has no reuse.
        checks = batch(temp/'benchmark', [(list(range(5))*100,100)])
        assert checks == [[495]*6]
        manifest = []
        for name, x in traces.items():
            raw = encode(x,100).encode()
            (OUT/'data'/f'{name}.txt').write_bytes(raw)
            counts = Counter(x)
            manifest.append(dict(dataset=name, title=TITLES[name], seed=0, requests=len(x),
                                 unique_keys=len(counts), sha256=hashlib.sha256(raw).hexdigest()))
        (OUT/'data/manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
        capacity_jobs = [(name, c, x) for name,x in traces.items() for c in (50,100,150,200)]
        capacity_hits = batch(temp/'benchmark', [(x,c) for _,c,x in capacity_jobs])
        capacity_rows = [dict(dataset=name, capacity=c, algorithm=a, hits=h, requests=len(x), hit_rate=h/len(x))
                         for (name,c,x),hs in zip(capacity_jobs,capacity_hits) for a,h in zip(NAMES,hs)]
        write_csv('capacity.csv',capacity_rows)
        # An extra budget check: 2Q(143) stores floor(.1*143)+floor(.6*143)=99 values.
        equal2q = batch(temp/'benchmark', [(x,143) for x in traces.values()])
        write_csv('twoq_budget.csv', [dict(dataset=name, nominal_capacity=143, resident_capacity=99, hits=h[3])
                                     for name,h in zip(traces,equal2q)])
        configurations = {'LRU100': [('LRU',100)], 'LRU50_LRU50': [('LRU',50),('LRU',50)],
                          'LRU20_LFU80':[('LRU',20),('LFU',80)], 'LRU20_ARC80':[('LRU',20),('ARC',80)],
                          'LRU20_LIRS80':[('LRU',20),('LIRS',80)]}
        hierarchy_rows = []
        for label, layers in configurations.items():
            path = OUT/'configs'/f'{label}.json'
            path.write_text(json.dumps({'cache':[dict(name=a,size=c) for a,c in layers]},indent=2)+'\n')
            results = batch(temp/'hierarchy', [(x,100) for x in traces.values()], [path])
            for (name,x),h in zip(traces.items(),results):
                hierarchy_rows.append(dict(dataset=name, configuration=label, requests=len(x), hits=h[0],
                                           misses=len(x)-h[0], hit_rate=h[0]/len(x)))
                if label == 'LRU100': assert h[0] == primary[name][0]
        write_csv('hierarchy.csv', hierarchy_rows)
    metadata = {'base_commit': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                'compiler': subprocess.check_output([args.cxx,'--version'],text=True).splitlines()[0],
                'flags': flags[:4], 'python': platform.python_version(), 'platform':platform.platform(),
                'seeds':list(range(10)), 'main_capacity':100}
    (OUT/'data/environment.json').write_text(json.dumps(metadata,indent=2)+'\n')
    draw(traces,primary,rows,capacity_rows,hierarchy_rows)
    for name,h in primary.items(): print(name,len(traces[name]),h)


def draw(traces, primary, rows, capacity_rows, hierarchy_rows):
    os.environ.setdefault('MPLCONFIGDIR', tempfile.gettempdir()+'/cache-lab-matplotlib')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,
                         'axes.spines.right':False,'figure.facecolor':'white'})
    colors=['#3b6fb6','#e29b31','#3c9c80','#9068b7','#d56569','#88939c']
    def save(fig,name):
        fig.savefig(OUT/'figures'/f'{name}.png',dpi=160,bbox_inches='tight')
        plt.close(fig)
    fig,axes=plt.subplots(1,3,figsize=(15,4),layout='constrained')
    for ax,name in zip(axes,['uniform','zipf','normal']):
        counts=Counter(traces[name])
        ax.bar(range(400),[counts[k] for k in range(400)],color='#3b6fb6',width=1)
        ax.set(title=TITLES[name],xlabel='Ключ',ylabel='Число обращений')
    save(fig,'probability_distributions')
    for name,x in traces.items():
        fig,ax=plt.subplots(figsize=(9,4.4),layout='constrained')
        bars=ax.bar(NAMES,primary[name],color=colors)
        ax.bar_label(bars,padding=3,fontsize=10)
        ax.set_ylim(0,max(primary[name])*1.17 if max(primary[name]) else 1)
        ax.set_ylabel('Количество хитов')
        ax.set_title(f'{TITLES[name]}\nC = 100; N = {len(x):,}; seed = 0')
        ax.grid(axis='y',alpha=.2); ax.set_axisbelow(True)
        save(fig,f'hits_{name}')
    fig,axes=plt.subplots(5,2,figsize=(14,18),layout='constrained')
    for ax,(name,x) in zip(axes.flat,traces.items()):
        frequencies=sorted(Counter(x).values(),reverse=True)
        ax.plot(range(1,len(frequencies)+1),frequencies,color='#3b6fb6')
        ax.set(xlabel='Ранг ключа по частоте',ylabel='Число обращений',title=TITLES[name],yscale='log')
        ax.grid(alpha=.2)
    save(fig,'distributions')
    fig,axes=plt.subplots(5,2,figsize=(14,18),layout='constrained')
    for ax,(name,x) in zip(axes.flat,traces.items()):
        ax.scatter(range(0,len(x),max(1,len(x)//2500)),x[::max(1,len(x)//2500)],s=2,alpha=.6,color='#3b6fb6')
        ax.set(xlabel='Номер обращения',ylabel='Ключ',title=TITLES[name])
    save(fig,'traces')
    fig,axes=plt.subplots(2,3,figsize=(15,8),layout='constrained')
    for ax,name in zip(axes.flat,['lru','lfu','arc','twoq','lirs','belady']):
        for a,color in zip(NAMES,colors):
            rs=[r for r in capacity_rows if r['dataset']==name and r['algorithm']==a]
            ax.plot([r['capacity'] for r in rs],[r['hit_rate']*100 for r in rs],'-o',label=a,color=color)
        ax.set(title=TITLES[name],xlabel='Ёмкость C',ylabel='Хиты, %',ylim=(-2,102))
        ax.grid(alpha=.2)
    axes.flat[0].legend(fontsize=8,ncol=2)
    save(fig,'capacity')
    fig,axes=plt.subplots(2,3,figsize=(16,9),layout='constrained')
    for ax,name in zip(axes.flat,['lru','lfu','arc','twoq','lirs','belady']):
        rs=[r for r in hierarchy_rows if r['dataset']==name]
        bars=ax.bar([r['configuration'].replace('_','\n') for r in rs],[r['hits'] for r in rs],color=colors[:5])
        ax.bar_label(bars,padding=2,fontsize=8)
        ax.tick_params(axis='x',labelsize=8)
        ax.set(title=TITLES[name],ylabel='Количество хитов')
        ax.set_ylim(0,max(r['hits'] for r in rs)*1.2)
    save(fig,'hierarchy')

if __name__ == '__main__': main()

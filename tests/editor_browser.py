"""Exercise the shipped configurator with a 64-byte WebHID fixture in Chromium."""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from shutil import which
from threading import Thread

from playwright.sync_api import expect, sync_playwright


class QuietServer(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


root = Path(__file__).resolve().parents[1] / 'web-configurator'
with ThreadingHTTPServer(('127.0.0.1', 0), partial(QuietServer, directory=root)) as server, sync_playwright() as pw:
    Thread(target=server.serve_forever, daemon=True).start()
    chrome = pw.chromium.launch(executable_path=which('google-chrome') or which('chromium'))
    page = chrome.new_page(viewport={'width': 1000, 'height': 800})
    errors = []
    page.on('pageerror', lambda exc: errors.append(str(exc)))
    page.goto(f'http://127.0.0.1:{server.server_port}/')
    page.evaluate("""async () => {
        const { defaults } = await import('./model.mjs');
        const { encodeRecord } = await import('./effects.mjs');
        const cfg = defaults();
        const rows = cfg.profiles.map(p => [p.socd, p.output, p.debounce, ...p.map]);
        const fx = Array.from({length:7}, (_,i) => encodeRecord(cfg.effects,i));
        class Board extends EventTarget {
            productId=3;
            async open(){ this.opened=true; }
            async close(){ this.opened=false; }
            async sendFeatureReport(id,q) {
                if (id || q.length!==64) throw Error('report size');
                const op=q[3], n=q[7];
                this.r=new Uint8Array(64); this.r.set(q.slice(0,8));
                if (op===32) { this.r.set([1,183,70,50,218,40,1],7); return; }
                if (op===1) {this.r.set(rows[n],10);this.r[31]=12;}
                if (op===2) this.draft=[structuredClone(rows),structuredClone(fx)];
                if (op===3) this.draft[0][n]=[...q.slice(10,31)];
                if (op===6) this.r.set(fx[n],10);
                if (op===7) this.draft[1][n]=[...q.slice(10,n?61:28)];
                if (op===4) { rows.splice(0,4,...this.draft[0]);fx.splice(0,7,...this.draft[1]); }
            }
            async receiveFeatureReport(){ return new DataView(this.r.buffer); }
        }
        const hid=new EventTarget(), board=new Board();
        hid.requestDevice=async()=>[board];
        Object.defineProperty(navigator,'hid',{value:hid,configurable:true});
    }""")
    page.locator('#connect').click()
    expect(page.locator('#status')).to_contain_text('已读取')
    page.locator('#mapping').select_option('26')
    page.locator('#accessory').select_option(index=3)
    page.locator('#led-map select').nth(4).select_option('3')
    page.locator('#rgbMode').select_option(index=1)
    page.locator('#count').fill('18')
    page.locator('#hz').fill('30')
    page.locator('#allow label').filter(has_text='LT').click()
    page.locator('#macro-index').select_option('5')
    page.locator('#macro-mode').select_option(index=2)
    page.locator('#add-step').click()
    page.locator('#steps').get_by_role('checkbox', name='RT', exact=True).check()
    page.locator('#steps input[type=number]').first.fill('321')
    page.locator('#save-fx').click()
    expect(page.locator('#status')).to_contain_text('已写入')
    page.locator('#connect').click()
    page.locator('#connect').click()
    expect(page.locator('#status')).to_contain_text('已读取')
    expect(page.locator('#steps input[type=number]').first).to_have_value('321')
    expect(page.locator('#steps').get_by_role('checkbox',name='RT',exact=True)).to_be_checked()
    expect(page.locator('#led-map select').nth(4)).to_have_value('3')
    expect(page.locator('#count')).to_have_value('18')
    expect(page.locator('#hz')).to_have_value('30')
    expect(page.locator('#mapping')).to_have_value('26')
    page.locator('#accessory').select_option(index=2)
    expect(page.locator('#address')).to_be_visible()
    expect(page.locator('#led-map')).not_to_be_visible()
    page.locator('#steps button').first.click()
    expect(page.locator('#steps .step')).to_have_count(0)
    assert not errors, errors
    chrome.close()
    server.shutdown()
    print('Browser editor, HID commit/readback, reconnect and shared-pin controls passed')

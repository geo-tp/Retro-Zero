#include "rom_upload_webui.h"
#include "../Core/core_registry.h"

#include <sstream>
#include <string>

namespace {
std::string html_escape(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}
}

// Generates the self-contained browser UI used by the ROM upload server.
std::string build_rom_upload_webui_html(
    const std::vector<std::pair<std::string, std::string>> &systems)
{
    std::ostringstream opts;
    opts << "<option value=\"\" selected disabled>-- Select a system --</option>\n";
    for (const auto &item : systems) {
        opts << "<option value=\"" << html_escape(item.first) << "\">"
             << html_escape(item.first) << " - " << html_escape(item.second)
             << "</option>\n";
    }

    std::ostringstream format_rows;
    for (const CoreConfig &core : CoreRegistry::all()) {
        if (!core.id || !core.id[0] || core.extensions.empty()) {
            continue;
        }

        std::ostringstream exts;
        for (size_t i = 0; i < core.extensions.size(); ++i) {
            if (i > 0) {
                exts << ", ";
            }
            exts << "<code>" << html_escape(core.extensions[i]) << "</code>";
        }

        format_rows << "<tr>"
                    << "<td><strong>" << html_escape(core.shortName ? core.shortName : "") << "</strong></td>"
                    << "<td>" << html_escape(core.name ? core.name : "") << "</td>"
                    << "<td>" << exts.str() << "</td>"
                    << "<td><code>" << html_escape(core.id ? core.id : "") << "</code></td>"
                    << "</tr>";
    }

    std::ostringstream ext_rules;
    ext_rules << "const allowedExts={";

    bool first_core_ext = true;
    for (const CoreConfig &core : CoreRegistry::all()) {
        if (!core.id || !core.id[0] || core.extensions.empty()) {
            continue;
        }

        if (!first_core_ext) {
            ext_rules << ",";
        }
        first_core_ext = false;

        ext_rules << "\"" << core.id << "\":[";

        for (size_t i = 0; i < core.extensions.size(); ++i) {
            if (i > 0) {
                ext_rules << ",";
            }
            ext_rules << "\"" << core.extensions[i] << "\"";
        }

        ext_rules << "]";
    }

    ext_rules << "};";

    return std::string(
        "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>CP0 ROM Upload</title><style>"
        ":root{--bg:#0d1218;--card:#19212b;--line:#2a3a4c;--text:#e9f1fb;--muted:#a9bfd6;--accent:#53b2ff;--accent2:#78d4ff;--ink:#0b1520;}"
        "*{box-sizing:border-box}"
        "body{font-family:Segoe UI,Arial,sans-serif;color:var(--text);margin:0;padding:18px;"
        "background-color:var(--bg);"
        "background-image:radial-gradient(1200px 700px at 10% -10%,#1f3a57 0%,rgba(31,58,87,0) 65%),"
        "radial-gradient(1000px 620px at 105% 0%,#22364d 0%,rgba(34,54,77,0) 60%),"
        "radial-gradient(900px 560px at 50% 120%,#1a2b3d 0%,rgba(26,43,61,0) 64%),"
        "linear-gradient(160deg,#0c1218 0%,#111a24 45%,#0e141b 100%);"
        "background-repeat:no-repeat;"
        "background-attachment:fixed;}"
        "body::before{content:'';position:fixed;inset:-20vmax;pointer-events:none;opacity:.2;"
        "background:radial-gradient(closest-side,#89d1ff 0%,rgba(137,209,255,0) 70%) -30vmax -20vmax/70vmax 70vmax no-repeat,"
        "radial-gradient(closest-side,#4d8fce 0%,rgba(77,143,206,0) 70%) 95vmax 15vmax/55vmax 55vmax no-repeat;"
        "filter:blur(26px);}"
        ".box{max-width:760px;margin:auto;background:linear-gradient(160deg,#1a2330,#161d27);border:1px solid var(--line);border-radius:14px;padding:18px;"
        "box-shadow:0 18px 44px rgba(4,10,16,.45), inset 0 1px 0 rgba(255,255,255,.04);}"
        "h2{margin:0 0 8px 0}.muted{margin:0 0 14px 0;color:var(--muted)}"
        ".notes{margin:0 0 16px 0;padding:12px 14px;border-radius:10px;border:1px solid #31465d;background:linear-gradient(180deg,#14202c,#101923);color:#c8d8ea}"
        ".notes strong{color:#eef6ff}.notes ul{margin:8px 0 0 18px;padding:0}.notes li{margin:6px 0}"
        ".formats{margin:0 0 16px 0;border:1px solid #31465d;border-radius:10px;background:#101923;overflow:hidden}"
        ".formats summary{cursor:pointer;padding:12px 14px;color:#eef6ff;font-weight:700;list-style:none}"
        ".formats summary::-webkit-details-marker{display:none}"
        ".formats summary::after{content:'+';float:right;color:#78d4ff;font-size:18px;line-height:18px}"
        ".formats[open] summary::after{content:'-'}"
        ".formats .formats-body{border-top:1px solid #26394d;padding:10px 12px 12px 12px;overflow:auto}"
        ".formats table{width:100%;border-collapse:collapse;font-size:13px;min-width:520px}"
        ".formats th,.formats td{border-bottom:1px solid #26394d;padding:8px 7px;text-align:left;vertical-align:top}"
        ".formats th{color:#9fc8eb;font-weight:700;background:#121d29}"
        ".formats code{display:inline-block;margin:1px 2px;padding:2px 5px;border-radius:5px;background:#0a121b;border:1px solid #284159;color:#dff3ff}"
        "label{display:block;margin:10px 0 6px 0;color:#b7c9dd;font-weight:600}"
        "select,input,button{width:100%;padding:10px;border-radius:8px;border:1px solid #35495f;background:#0f1722;color:#e8f1fb;}"
        "select:focus,input:focus,button:focus{outline:none;border-color:#6cbfff;box-shadow:0 0 0 2px rgba(90,176,255,.22);}"
        "input[type=file]{padding:8px 10px;background:linear-gradient(180deg,#0f1a26,#0c1520);border:1px solid #395068;}"
        "input[type=file]::file-selector-button{margin:-2px 12px -2px -2px;padding:8px 14px;border-radius:7px;border:1px solid #4e95d1;"
        "background:linear-gradient(180deg,var(--accent2),var(--accent));color:var(--ink);font-weight:700;cursor:pointer;transition:all .15s ease;}"
        "input[type=file]::file-selector-button:hover{filter:brightness(1.05);transform:translateY(-1px);}"
        "input[type=file]::-webkit-file-upload-button{margin:-2px 12px -2px -2px;padding:8px 14px;border-radius:7px;border:1px solid #4e95d1;"
        "background:linear-gradient(180deg,var(--accent2),var(--accent));color:var(--ink);font-weight:700;cursor:pointer;}"
        "button{margin-top:12px;background:linear-gradient(180deg,var(--accent2),var(--accent));color:var(--ink);font-weight:700;cursor:pointer;border-color:#4e95d1;}"
        "button:hover{filter:brightness(1.04)}"
        "button:disabled{cursor:not-allowed;filter:grayscale(.25);opacity:.55}"
        "#progressWrap{display:none;margin-top:10px}"
        "#progressText{font-size:12px;color:#9fc8eb;margin:0 0 6px 0}"
        "#progressTrack{width:100%;height:8px;border-radius:999px;background:#0c1622;border:1px solid #2f4963;overflow:hidden}"
        "#progressBar{height:100%;width:0%;background:linear-gradient(90deg,#63c4ff,#3b98de);transition:width .08s linear}"
        "#msg{margin-top:12px;white-space:pre-wrap;color:#b9dfff;min-height:20px;max-height:240px;overflow:auto;background:#121b26;border:1px solid #2a3c53;border-radius:8px;padding:10px}"
        "</style></head><body>"
        "<div class=\"box\">"
        "<h2>Retro Zero Upload</h2>"
        "<p class=\"muted\">Destination: /home/pi/roms/&lt;system&gt;. You can upload multiple files in one go.</p>"
        "<div class=\"notes\"><strong>Format notes</strong><ul>"
        "<li>Some systems, especially arcade cores, can load zipped ROMs directly, but as a general rule it is recommended to unzip your ROMs.</li>"
        "<li>For FBNeo (arcade), use an <strong>FBNeo-compatible ROM set.</strong></li>"
        "<li>For Neo Geo, keep <strong>neogeo.zip</strong> in the Neo Geo ROM folder.</li>"
        "<li>For MAME 2010, use ROMs from set <strong>0.139</strong>. Some games may also require BIOS files.</li>"
        "</ul></div>"
        "<details class=\"formats\">"
        "<summary>Supported ROM formats</summary>"
        "<div class=\"formats-body\">"
        "<table>"
        "<thead><tr><th>Short</th><th>System</th><th>Supported formats</th><th>Folder</th></tr></thead>"
        "<tbody>" + format_rows.str() + "</tbody>"
        "</table>"
        "</div>"
        "</details>"
        "<label for=\"sys\">System</label><select id=\"sys\" required>" + opts.str() + "</select>"
        "<label for=\"files\">ROM files</label><input id=\"files\" type=\"file\" multiple>"
        "<button id=\"up\" disabled>Upload Selected Files</button>"
        "<div id=\"progressWrap\"><p id=\"progressText\">Preparing upload...</p><div id=\"progressTrack\"><div id=\"progressBar\"></div></div></div>"
        "<div id=\"msg\">Select a system to enable upload.</div></div>"
        "<script>" + ext_rules.str() +
        "const btn=document.getElementById('up');"
        "const msg=document.getElementById('msg');"
        "const filesIn=document.getElementById('files');"
        "const sys=document.getElementById('sys');"
        "const progressWrap=document.getElementById('progressWrap');"
        "const progressText=document.getElementById('progressText');"
        "const progressBar=document.getElementById('progressBar');"
        "const updateState=()=>{"
        "const ok=!!sys.value;"
        "btn.disabled=!ok;"
        "if(ok&&msg.textContent==='Select a system to enable upload.')msg.textContent='Ready.';"
        "};"
        "sys.addEventListener('change',updateState);"
        "updateState();"
        "const setProgress=(pct,label)=>{"
        "const v=Math.max(0,Math.min(100,pct));"
        "progressBar.style.width=v.toFixed(1)+'%';"
        "progressText.textContent=label+' ('+v.toFixed(1)+'%)';"
        "};"
        "const line=(t)=>{msg.textContent += (msg.textContent==='Ready.'?'':'\\n') + t;};"
        "const uploadOne=(url,file,onProgress)=>new Promise((resolve,reject)=>{"
        "const xhr=new XMLHttpRequest();"
        "xhr.open('PUT',url,true);"
        "xhr.upload.onprogress=(ev)=>{if(ev.lengthComputable)onProgress(ev.loaded,ev.total);};"
        "xhr.onload=()=>resolve({ok:xhr.status>=200&&xhr.status<300,text:xhr.responseText,status:xhr.status});"
        "xhr.onerror=()=>reject(new Error('Network error'));"
        "xhr.send(file);"
        "});"
        "const getExt=(name)=>{"
        "const n=(name||'').toLowerCase();"
        "const slash=Math.max(n.lastIndexOf('/'),n.lastIndexOf('\\\\'));"
        "const base=slash>=0?n.slice(slash+1):n;"
        "const dot=base.lastIndexOf('.');"
        "return dot>=0?base.slice(dot):'';"
        "};"
        "btn.onclick=async()=>{"
        "if(!sys.value){msg.textContent='Select a system first.';return;}"
        "const files=filesIn.files;"
        "if(!files||files.length===0){msg.textContent='Select one or more files first.';return;}"
        "const allowed=allowedExts[sys.value]||[];"
        "const unsupported=[];"
        "for(let i=0;i<files.length;i++){"
        "const name=files[i].name||'';"
        "const ext=getExt(name);"
        "if(allowed.length&&allowed.indexOf(ext)<0){"
        "unsupported.push(name+'  ('+(ext||'no extension')+')');"
        "}"
        "}"
        "if(unsupported.length){"
        "const ok=confirm("
        "'Some files do not match the selected system.\\n\\n'+"
        "'Selected system: '+sys.value+'\\n'+"
        "'Expected formats: '+(allowed.length?allowed.join(', '):'unknown')+'\\n\\n'+"
        "'Suspicious files:\\n- '+unsupported.join('\\n- ')+'\\n\\n'+"
        "'Upload anyway?'"
        ");"
        "if(!ok){msg.textContent='Upload cancelled. Select the correct system or choose matching ROM files.';return;}"
        "}"
        "btn.disabled=true;sys.disabled=true;filesIn.disabled=true;"
        "progressWrap.style.display='block';"
        "msg.textContent='';"
        "let doneBytes=0;"
        "let totalBytes=0;for(let i=0;i<files.length;i++)totalBytes+=files[i].size||0;"
        "if(totalBytes<=0)totalBytes=1;"
        "for(let i=0;i<files.length;i++){"
        "const f=files[i];"
        "const u='/upload?sys='+encodeURIComponent(sys.value)+'&name='+encodeURIComponent(f.name);"
        "line('['+(i+1)+'/'+files.length+'] Uploading '+f.name+' ...');"
        "setProgress((doneBytes*100)/totalBytes,'Uploading '+f.name);"
        "try{"
        "const r=await uploadOne(u,f,(loaded)=>{"
        "const pct=((doneBytes+loaded)*100)/totalBytes;"
        "setProgress(pct,'Uploading '+f.name);"
        "});"
        "doneBytes+=f.size||0;"
        "setProgress((doneBytes*100)/totalBytes,'Processed '+(i+1)+'/'+files.length+' file(s)');"
        "line((r.ok?'OK: ':'ERR: ')+(r.text||('HTTP '+r.status)));"
        "}catch(e){line('ERR: '+f.name+' ('+e+')');}"
        "}"
        "setProgress(100,'Upload finished');"
        "btn.disabled=!sys.value;sys.disabled=false;filesIn.disabled=false;"
        "};"
        "</script></body></html>");
}
$ErrorActionPreference = "Stop"
$root = "C:\Users\luec-a\Documents\CapCom\captain-web"
Stop-Process -Name captain-web -Force -ErrorAction SilentlyContinue
Copy-Item "$PSScriptRoot\src\storage\local_yaml_storage.cpp" "$root\src\storage\local_yaml_storage.cpp" -Force

$apiPath = "$root\src\api\api_server.cpp"
$api = [IO.File]::ReadAllText($apiPath)
if ($api -notmatch 'body\.contains\("attributes"\)') {
  $createNeedle = 'auto created = repository_.create(value);'
  $createInsert = @'
auto created = repository_.create(value);
            if (body.contains("attributes") && body.at("attributes").is_object()) {
                for (const auto& [key, attribute] : body.at("attributes").items()) {
                    created.attributes[key] = attribute.is_string()
                        ? attribute.get<std::string>()
                        : attribute.dump();
                }
                created.updated_by = user();
                created = repository_.update(created);
            }
'@
  if (-not $api.Contains($createNeedle)) { throw "Create API marker not found." }
  $api = $api.Replace($createNeedle, $createInsert.TrimEnd())

  $updateNeedle = 'found->rationale = body.value("rationale", found->rationale);'
  $updateInsert = @'
found->rationale = body.value("rationale", found->rationale);
            if (body.contains("attributes") && body.at("attributes").is_object()) {
                for (const auto& [key, attribute] : body.at("attributes").items()) {
                    found->attributes[key] = attribute.is_string()
                        ? attribute.get<std::string>()
                        : attribute.dump();
                }
            }
'@
  if (-not $api.Contains($updateNeedle)) { throw "Update API marker not found." }
  $api = $api.Replace($updateNeedle, $updateInsert.TrimEnd())
  [IO.File]::WriteAllText($apiPath,$api,[Text.UTF8Encoding]::new($false))
}

$indexPath = "$root\web\index.html"
$html = [IO.File]::ReadAllText($indexPath)
if ($html -notmatch 'id="dynamicAttributes"') {
  $marker = '<div id="links" class="grid grid-cols-2 gap-3">'
  if (-not $html.Contains($marker)) { throw "Form link marker not found." }
  $panel = '<section id="dynamicAttributes" class="rounded border border-slate-200 bg-slate-50 p-4"><h3 class="font-bold text-blue-900">Typspezifische Angaben</h3><div id="attributeFields" class="mt-3 grid gap-3 md:grid-cols-2"></div></section>'
  $html = $html.Replace($marker,$panel+$marker)

  $scriptMarker = "function vals(e){return[...e.selectedOptions].map(x=>x.value)}"
  if (-not $html.Contains($scriptMarker)) { throw "JavaScript marker not found." }
  $dynamic = @'
function vals(e){return[...e.selectedOptions].map(x=>x.value)}
const ATTRIBUTE_SCHEMAS={
 CLASS:[['software_system','Softwaresystem','text'],['software_safety_class','Software-Sicherheitsklasse','class'],['classification_responsible','Klassifizierungsverantwortlich','text'],['classification_rationale','Klassifizierungsbegründung','textarea'],['risk_management_document','Risikomanagementdokument','text'],['risk_management_record_ids','Risikomanagementeinträge','RISK']],
 RISK:[['hazard','Gefährdung','textarea'],['hazardous_situation','Gefährdungssituation','textarea'],['harm','Schaden','textarea'],['risk_responsible','Risikoverantwortlich','text'],['initial_severity','Anfänglicher Schweregrad','text'],['initial_probability','Anfängliche Wahrscheinlichkeit','text'],['control_requirements','Risikokontrollanforderungen','SRS'],['verification_tests','Verifikationstests','TEST']],
 SRS:[['classification_ref','Klassifizierungsreferenz','CLASS'],['software_item_safety_class','Software-Sicherheitsklasse','class'],['classification_responsible','Klassifizierungsverantwortlich','text'],['classification_rationale','Klassifizierungsbegründung','textarea'],['risk_references','Risikoreferenzen','RISK']],
 SEC:[['classification_ref','Klassifizierungsreferenz','CLASS'],['software_item_safety_class','Software-Sicherheitsklasse','class'],['classification_responsible','Klassifizierungsverantwortlich','text'],['classification_rationale','Klassifizierungsbegründung','textarea'],['risk_references','Risikoreferenzen','RISK']],
 UT:[['verifies','Verifiziert Anforderungen','VERIFY'],['test_method','Testmethode','textarea'],['expected_result','Erwartetes Ergebnis','textarea'],['test_responsible','Testverantwortlich','text']],
 IT:[['verifies','Verifiziert Anforderungen','VERIFY'],['test_method','Testmethode','textarea'],['expected_result','Erwartetes Ergebnis','textarea'],['test_responsible','Testverantwortlich','text']],
 ST:[['verifies','Verifiziert Anforderungen','VERIFY'],['test_method','Testmethode','textarea'],['expected_result','Erwartetes Ergebnis','textarea'],['test_responsible','Testverantwortlich','text']],
 AT:[['verifies','Verifiziert Anforderungen','VERIFY'],['test_method','Testmethode','textarea'],['expected_result','Erwartetes Ergebnis','textarea'],['test_responsible','Testverantwortlich','text']],
 TEST:[['verifies','Verifiziert Anforderungen','VERIFY'],['test_method','Testmethode','textarea'],['expected_result','Erwartetes Ergebnis','textarea'],['test_responsible','Testverantwortlich','text']]
};
function parseAttributeList(value){const text=String(value||'').trim();if(!text.startsWith('['))return[];return text.slice(1,-1).split(',').map(x=>x.trim().replace(/^"|"$/g,'')).filter(Boolean)}
function candidates(kind){if(kind==='VERIFY')return S.items.filter(x=>['SRS','SEC','SYS'].includes(x.type));if(kind==='TEST')return S.items.filter(x=>['UT','IT','ST','AT','TEST'].includes(x.type));return S.items.filter(x=>x.type===kind)}
function renderAttributes(type,values={}){const fields=ATTRIBUTE_SCHEMAS[type]||[];$('dynamicAttributes').classList.toggle('hidden',fields.length===0);$('attributeFields').innerHTML=fields.map(([key,label,kind])=>{const value=values[key]||'';if(kind==='textarea')return `<label class="text-sm font-semibold md:col-span-2">${label} *<textarea data-attr="${key}" rows="2" class="mt-1 w-full rounded border p-2 font-normal">${esc(value)}</textarea></label>`;if(kind==='class')return `<label class="text-sm font-semibold">${label} *<select data-attr="${key}" class="mt-1 w-full rounded border p-2 font-normal"><option value="">Bitte wählen</option>${['A','B','C'].map(v=>`<option ${v===value?'selected':''}>${v}</option>`).join('')}</select></label>`;if(['CLASS','RISK','SRS','VERIFY','TEST'].includes(kind)){const selected=parseAttributeList(value);const multiple=!['CLASS'].includes(kind);return `<label class="text-sm font-semibold">${label} *<select data-attr="${key}" ${multiple?'multiple':''} class="mt-1 h-28 w-full rounded border p-2 font-normal">${!multiple?'<option value="">Bitte wählen</option>':''}${candidates(kind).map(item=>`<option value="${item.uid}" ${selected.includes(item.uid)||value===item.uid?'selected':''}>${item.uid} · ${esc(short(item.title))}</option>`).join('')}</select></label>`}return `<label class="text-sm font-semibold">${label} *<input data-attr="${key}" value="${esc(value)}" class="mt-1 w-full rounded border p-2 font-normal"></label>`}).join('')}
function attributePayload(){const result={};document.querySelectorAll('[data-attr]').forEach(field=>{if(field.multiple)result[field.dataset.attr]=`[${[...field.selectedOptions].map(x=>x.value).join(', ')}]`;else result[field.dataset.attr]=field.value});return result}
'@
  $html = $html.Replace($scriptMarker,$dynamic.TrimEnd())

  $html = $html.Replace("function newReq(){S.editing=false;`$('reqForm').reset();`$('reqHeading').textContent='Neue Anforderung';", "function newReq(){S.editing=false;`$('reqForm').reset();`$('reqHeading').textContent='Neue Anforderung';renderAttributes(`$('type').value,{});")
  $html = $html.Replace("`$('rationale').value=x.rationale||'';", "`$('rationale').value=x.rationale||'';renderAttributes(x.type,x.attributes||{});")
  $html = $html.Replace("const body={type:`$('type').value,title:`$('title').value,text:`$('text').value,rationale:`$('rationale').value,parents:vals(`$('parents')),children:vals(`$('children'))};", "const body={type:`$('type').value,title:`$('title').value,text:`$('text').value,rationale:`$('rationale').value,parents:vals(`$('parents')),children:vals(`$('children')),attributes:attributePayload()};")
  $html = $html.Replace("`$('new').onclick=newReq;", "`$('type').onchange=()=>renderAttributes(`$('type').value,{});`$('new').onclick=newReq;")
  [IO.File]::WriteAllText($indexPath,$html,[Text.UTF8Encoding]::new($false))
}
Write-Host "Captain Web v0.8 dynamic forms installed. Rebuild captain-web."

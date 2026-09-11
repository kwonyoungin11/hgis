#include "HeritageIntranetFlow.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace {

// 미확인 단계의 스크립트는 반드시 "not-found" 를 돌려줄 수 있어야 한다.
// 못 찾았는데 조용히 넘어가면 빈 자료를 성공으로 처리하게 된다.
// 이 사이트는 프레임을 쓴다. 다운로드 패널과 그 함수(searchGisChaRirList 등)는
// 바깥 창이 아니라 안쪽 프레임에 있다. runJavaScript 는 최상위 창에서만 도므로
// 그대로 두면 아무것도 못 찾는다(2026-09-11 실제 화면으로 확인).
//
// 그래서 모든 스크립트를 **다운로드 패널이 있는 창**에서 돌린다.
// W = 고른 창, document = 그 창의 document 로 가려 두면 기존 스크립트가 그대로 동작한다.
// 같은 출처라 프레임 접근이 막히지 않는다.
QString wrap(const QString& body) {
  // 2026-09-11 실패 outline(main.do): 바깥 창에 searchGisChaRirList·전체다운로드 메뉴가
  // 있고, 다운로드 폼(codedeta)은 없다. 함수가 있는 창을 먼저 고르면 지도 칸(bjdcd)에
  // 머문다. 손자 프레임까지 보고, **codedeta 가 있는 창**을 먼저 고른다.
  // 사이트 함수는 바깥 창에 있을 수 있어 fn() 으로 찾는다.
  return QStringLiteral(
             "(function(){"
             "function walk(w,out,depth){"
             "if(!w||depth>6)return;"
             "out.push(w);"
             "try{for(var i=0;i<w.frames.length;i++){"
             "try{void w.frames[i].document.body;walk(w.frames[i],out,depth+1);}"
             "catch(e){}}}catch(e){}}"
             "function cands(){var out=[];walk(window,out,0);return out;}"
             "function hasCodedeta(w){"
             "try{return !!w.document.querySelector('select[name*=codedeta],select[id*=codedeta]');}"
             "catch(e){return false;}}"
             "function pick(){var c=cands();"
             "for(var i=0;i<c.length;i++){if(hasCodedeta(c[i]))return c[i];}"
             "for(var i=0;i<c.length;i++){"
             "try{var t=(c[i].document.body&&c[i].document.body.innerText)||'';"
             "if(t.indexOf('지정유산')>=0&&t.indexOf('발굴조사구역')>=0&&"
             "(t.indexOf('시/도')>=0||t.indexOf('시도')>=0))return c[i];}catch(e){}}"
             "for(var i=0;i<c.length;i++){"
             "try{var t=((c[i].document.body&&c[i].document.body.innerText)||'').replace(/\\s+/g,'');"
             "if(t.indexOf('서약서에동의')>=0)return c[i];}catch(e){}}"
             "for(var i=0;i<c.length;i++){"
             "try{if(typeof c[i].showAgreePopup==='function')return c[i];}catch(e){}}"
             "return window;}"
             "function fn(name){"
             "try{if(typeof W[name]==='function')return W[name];}catch(e){}"
             "var c=cands();for(var i=0;i<c.length;i++){"
             "try{if(typeof c[i][name]==='function')return c[i][name];}catch(e){}}"
             "return null;}"
             "var W=pick();var document=W.document;"
             "try{%1}catch(e){return 'error:'+String(e);}})()")
      .arg(body);
}

}  // namespace

QUrl HeritageIntranetFlow::portalUrl() {
  return QUrl(QStringLiteral("https://intranet.gis-heritage.go.kr/"));
}

QUrl HeritageIntranetFlow::loginUrl() {
  return QUrl(QStringLiteral("https://intranet.gis-heritage.go.kr/user/checkNet.do"));
}

QString HeritageIntranetFlow::stageName(HeritageStage stage) {
  switch (stage) {
    case HeritageStage::Idle: return QStringLiteral("대기");
    case HeritageStage::Login: return QStringLiteral("로그인");
    case HeritageStage::DismissTutorial: return QStringLiteral("튜토리얼 닫기");
    case HeritageStage::OpenDownloadPage: return QStringLiteral("국가유산자료 다운로드 열기");
    case HeritageStage::AgreeTerms: return QStringLiteral("서약서 동의");
    case HeritageStage::SelectDataset: return QStringLiteral("자료 종류 선택");
    case HeritageStage::SelectRegion: return QStringLiteral("시·군 선택");
    case HeritageStage::Search: return QStringLiteral("검색");
    case HeritageStage::CollectResults: return QStringLiteral("결과 모으기");
    case HeritageStage::Download: return QStringLiteral("내려받기");
    case HeritageStage::Done: return QStringLiteral("완료");
    case HeritageStage::Failed: return QStringLiteral("실패");
  }
  return QStringLiteral("알 수 없음");
}

bool HeritageIntranetFlow::isVerified(HeritageStage stage) {
  // 2026-09-11 실제 화면에서 확인한 단계만 true. 나머지는 로그인 뒤 화면을 보고 채운다.
  switch (stage) {
    case HeritageStage::Idle:
    case HeritageStage::Login:
    case HeritageStage::Done:
    case HeritageStage::Failed:
      return true;
    default:
      return false;
  }
}

QString HeritageIntranetFlow::jsString(const QString& value) {
  // 직접 따옴표를 붙이지 않는다. 비밀번호에 역슬래시·따옴표·줄바꿈이 들어가도 깨지면 안 된다.
  const QByteArray encoded = QJsonDocument(QJsonArray{QJsonValue(value)}).toJson(QJsonDocument::Compact);
  // ["..."] 에서 바깥 대괄호만 벗긴다.
  const QString text = QString::fromUtf8(encoded).trimmed();
  if (text.size() >= 2 && text.startsWith(QLatin1Char('[')) && text.endsWith(QLatin1Char(']')))
    return text.mid(1, text.size() - 2);
  return QStringLiteral("\"\"");
}

QString HeritageIntranetFlow::loginScript(const QString& userId, const QString& password) {
  // 확인함(2026-09-11): 로그인 폼은 name="loginForm", action="/j_spring_security_check".
  // 보이는 칸은 #userId(text) 와 #pwd(password), 버튼은 onclick="goLogin();".
  // 숨은 칸 USER_ID/USER_PW 에는 페이지가 RSAModulus/RSAExponent 로 암호화한 값이 들어간다.
  // 그래서 우리가 암호화하지 않고 goLogin() 에 맡긴다. 평문 POST 는 로그인되지 않는다.
  return wrap(QStringLiteral(
                  "var id=document.getElementById('userId');"
                  "var pw=document.getElementById('pwd');"
                  "if(!id||!pw){return 'no-form';}"
                  "id.value=%1; pw.value=%2;"
                  "if(typeof goLogin!=='function'){return 'no-goLogin';}"
                  "goLogin(); return 'submitted';")
                  .arg(jsString(userId), jsString(password)));
}

QString HeritageIntranetFlow::loginProbeScript() {
  // 아직 로그인 칸이 보이면 못 들어간 것이다. 넘어갔다고 하지 않는다.
  return wrap(QStringLiteral(
      "if(document.getElementById('userId')&&document.getElementById('pwd'))"
      "{return 'still-login';}"
      "return 'past-login';"));
}

QString HeritageIntranetFlow::dismissTutorialScript() {
  // 로그인하면 매번 뜬다(2026-09-11 화면 확인). 우측 위 「튜토리얼 나가기」,
  // 가운데 안내창에 「다시 보지 않기」 · 1/5 · 이전/다음.
  // 이게 떠 있으면 상단 메뉴를 누를 수 없다. 닫힌 것을 확인하기 전에는 다음으로 가지 않는다.
  //
  // **보이는 것만 센다.** 닫아도 마크업이 숨은 채 남으면 영영 「아직 있다」가 되어 멈춘다.
  // 글자는 공백·줄바꿈을 지우고 비교하고, 처리기가 걸린 안쪽 요소를 누른다.
  return wrap(QStringLiteral(
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "function visible(el){if(!el)return false;"
      "if(el.offsetParent===null&&getComputedStyle(el).position!=='fixed')return false;"
      "var r=el.getBoundingClientRect();"
      "if(r.width<=0||r.height<=0)return false;"
      "var st=getComputedStyle(el);"
      "return st.visibility!=='hidden'&&st.display!=='none'&&parseFloat(st.opacity||'1')>0.05;}"
      "function findInner(mark){"
      "var all=document.querySelectorAll('button,a,span,div,li,p');var hit=null;"
      "for(var i=0;i<all.length;i++){"
      "if(squash(all[i].innerText).indexOf(mark)<0)continue;"
      "if(!visible(all[i]))continue;"
      "var inner=false;var kids=all[i].querySelectorAll('*');"
      "for(var k=0;k<kids.length;k++){"
      "if(visible(kids[k])&&squash(kids[k].innerText).indexOf(mark)>=0){inner=true;break;}}"
      "if(!inner){hit=all[i];}}"
      "return hit;}"
      "var exit=findInner('튜토리얼나가기');"
      "var again=findInner('다시보지않기');"
      "if(!exit&&!again)return 'none';"
      "if(again){try{again.click();}catch(e){}"
      "var box=again.querySelector('input[type=checkbox]')||"
      "(again.parentElement?again.parentElement.querySelector('input[type=checkbox]'):null);"
      "if(box&&!box.checked){box.checked=true;"
      "box.dispatchEvent(new Event('change',{bubbles:true}));}}"
      "if(exit){exit.click();return 'closed';}"
      "return 'closed';"));
}

QString HeritageIntranetFlow::openDownloadPageScript() {
  // 확인함(2026-09-11 DOM): 「국가유산자료 다운로드」 링크는 javascript:showAgreePopup(); 이다.
  // 메뉴를 펼쳐 누르는 것보다 함수를 직접 부르는 편이 확실하다. 화면이 ajax 로 바뀌어도 흔들리지 않는다.
  return wrap(QStringLiteral(
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "var open=fn('showAgreePopup');"
      "if(open){open();return 'opened';}"
      "var want='국가유산자료다운로드';"
      "var nodes=document.querySelectorAll('a,button,li');"
      "for(var i=0;i<nodes.length;i++){"
      "if(squash(nodes[i].innerText)!==want)continue;"
      "var href=nodes[i].getAttribute?nodes[i].getAttribute('href'):null;"
      "if(href&&href.toLowerCase().indexOf('javascript')===0){"
      "try{(new Function(href.slice(href.indexOf(':')+1))).call(nodes[i]);return 'opened';}catch(e){}}"
      "try{nodes[i].click();return 'opened';}catch(e){}}"
      "return 'not-found';"));
}

QString HeritageIntranetFlow::agreeTermsScript() {
  // 「위 서약서에 동의합니다」 체크 후 「확인」 (2026-09-11 화면 확인).
  // 동의 시각과 서약서 원문은 호출자가 영수증으로 남긴다. 조용히 지나가지 않는다.
  return wrap(QStringLiteral(
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "var boxes=document.querySelectorAll('input[type=checkbox]');"
      "var target=null;"
      "for(var i=0;i<boxes.length;i++){"
      "var around=boxes[i].parentElement?squash(boxes[i].parentElement.innerText):'';"
      "var labs=document.querySelectorAll('label');"
      "for(var k=0;k<labs.length;k++){"
      "if(boxes[i].id&&labs[k].htmlFor===boxes[i].id)around+=squash(labs[k].innerText);}"
      "if(around.indexOf('서약서에동의')>=0){target=boxes[i];break;}}"
      "if(!target&&boxes.length===1)target=boxes[0];"
      "if(!target){"
      "if(document.querySelector('select[name*=codedeta],select[id*=codedeta]'))"
      "return 'already-open';"
      "return 'not-found';}"
      "if(!target.checked){target.click();"
      "if(!target.checked){target.checked=true;"
      "target.dispatchEvent(new Event('change',{bubbles:true}));}}"
      "var btns=document.querySelectorAll('button,input[type=button],input[type=submit],a');"
      "for(var j=0;j<btns.length;j++){"
      "if(squash(btns[j].innerText||btns[j].value)==='확인'){btns[j].click();return 'agreed';}}"
      "return 'checked-no-confirm';"));
}

QString HeritageIntranetFlow::selectDatasetScript(HeritageDataset dataset) {
  // 미확인. 탭 6개. 사이트는 changeTab('B') 같은 코드로 가른다(지정유산 코드는 아직 모른다).
  const QString name = HeritageStyle::layerName(dataset);
  const QString code = HeritageStyle::tabCode(dataset);
  if (code.isEmpty()) {
    // 코드를 모르면 글자로 찾는다. 모르는 것을 아는 척하지 않는다.
    return wrap(QStringLiteral("var tabs=document.querySelectorAll('a,li,button');"
                               "for(var i=0;i<tabs.length;i++){"
                               "if((tabs[i].innerText||'').trim()===%1){tabs[i].click();return 'selected';}}"
                               "return 'not-found';")
                    .arg(jsString(name)));
  }
  return wrap(QStringLiteral("var tab=fn('changeTab');"
                             "if(!tab){return 'no-changeTab';}"
                             "tab(%1); return 'selected';")
                  .arg(jsString(code)));
}

QString HeritageIntranetFlow::selectRegionScript(const QString& sido, const QString& city) {
  // 화면에 시/도 select 가 **둘** 있다(2026-09-11 실제 DOM 확인):
  //   - id=bjdcd1_newJbn / name=bjdCdNewAddr  → 왼쪽 지도의 지번·도로명 검색. **여기가 아니다**
  //   - id=codedetaCd0   / name=codedetaCd    → 국가유산자료 다운로드 폼. **여기다**
  // DOM 순서로는 지도 것이 먼저라 첫 옵션 글자만 보고 고르면 엉뚱한 칸을 건드린다.
  // 그래서 다운로드 폼의 이름(codedeta)을 먼저 찾고, 시/군/구는 **같은 폼 안에서 그 다음 select** 로 잡는다.
  //
  // **시/군까지만 고른다.** 읍면동·리 칸은 건드리지 않는다.
  return wrap(QStringLiteral(
                  "function squash(t){return (t||'').replace(/\\s+/g,'');}"
                  "function core(t){var s=squash(t);"
                  "s=s.replace('특별자치도','').replace('특별자치시','')"
                  ".replace('특별시','').replace('광역시','');"
                  "if(s.length>2&&s.charAt(s.length-1)==='도')s=s.slice(0,-1);"
                  "var map={'전라북':'전북','전라남':'전남','경상북':'경북','경상남':'경남',"
                  "'충청북':'충북','충청남':'충남'};"
                  "return map[s]||s;}"
                  "function same(a,b){var x=squash(a),y=squash(b);"
                  "if(!x||!y)return false;"
                  "if(x===y||x.indexOf(y)>=0||y.indexOf(x)>=0)return true;"
                  "var cx=core(x),cy=core(y);"
                  "if(cx===cy)return true;"
                  "function merged(c,raw){return (c==='전남'||c==='광주')&&raw.indexOf('전남광주')>=0;}"
                  "return merged(cx,y)||merged(cy,x);}"
                  "function firstOption(sel){return sel.options.length?squash(sel.options[0].text):'';}"
                  "function key(sel){return ((sel.id||'')+' '+(sel.name||'')).toLowerCase();}"
                  "var all=Array.prototype.slice.call(document.querySelectorAll('select'));"
                  "var sidoAt=-1;"
                  // 1순위: 다운로드 폼의 이름(codedeta). 지도 패널(bjdcd)은 제외한다.
                  "for(var i=0;i<all.length;i++){"
                  "if(key(all[i]).indexOf('codedeta')>=0&&firstOption(all[i]).indexOf('시도선택')>=0){sidoAt=i;break;}}"
                  "if(sidoAt<0){for(var i=0;i<all.length;i++){"
                  "if(key(all[i]).indexOf('bjdcd')>=0)continue;"
                  "if(firstOption(all[i]).indexOf('시도선택')>=0){sidoAt=i;break;}}}"
                  "if(sidoAt<0)return 'not-found';"
                  "var sidoSel=all[sidoAt];"
                  // 시/군/구는 같은 폼 안에서 시/도 바로 뒤에 오는 select 다.
                  "var citySel=null;"
                  "for(var i=sidoAt+1;i<all.length;i++){"
                  "if(sidoSel.form&&all[i].form!==sidoSel.form)continue;"
                  "var f=firstOption(all[i]);"
                  "if(f.indexOf('시/군/구선택')>=0||f.indexOf('시군구선택')>=0||f.indexOf('시·군·구선택')>=0){citySel=all[i];break;}}"
                  "function pick(sel,text){if(!sel)return false;"
                  "for(var i=0;i<sel.options.length;i++){"
                  "if(!same(sel.options[i].text,text))continue;"
                  "if(sel.selectedIndex!==i){sel.selectedIndex=i;"
                  "sel.dispatchEvent(new Event('input',{bubbles:true}));"
                  "sel.dispatchEvent(new Event('change',{bubbles:true}));"
                  "if(typeof sel.onchange==='function'){try{sel.onchange();}catch(e){}}}"
                  "return true;}"
                  "return false;}"
                  "if(!pick(sidoSel,%1))return 'no-sido';"
                  "if(!citySel)return 'sigungu-pending';"
                  "if(pick(citySel,%2))return 'selected';"
                  "return 'sigungu:'+(citySel.options.length-1);")
                  .arg(jsString(sido), jsString(city)));
}

QString HeritageIntranetFlow::maximizePageSizeScript() {
  // 「10개씩 보기」를 가장 큰 값으로 바꿔 쪽 수를 줄인다. 없으면 그냥 넘어간다.
  return wrap(QStringLiteral(
      "var all=document.querySelectorAll('select');"
      "for(var i=0;i<all.length;i++){"
      "var txt=(all[i].options.length?all[i].options[0].text:'')||'';"
      "if(txt.indexOf('개씩')<0)continue;"
      "var best=0,bestN=-1;"
      "for(var j=0;j<all[i].options.length;j++){"
      "var n=parseInt((all[i].options[j].text||'').replace(/[^0-9]/g,''),10);"
      "if(!isNaN(n)&&n>bestN){bestN=n;best=j;}}"
      "if(bestN>0){all[i].selectedIndex=best;"
      "all[i].dispatchEvent(new Event('change',{bubbles:true}));return 'resized';}}"
      "return 'not-found';"));
}

QString HeritageIntranetFlow::formHintScript() {
  // wrap() 없이 이 프레임 document 만 본다. codedeta select 가 있는 창만 폼이다.
  // 지정유산·발굴조사구역 글자만으로는 지도/메뉴를 폼으로 오인한다.
  return QStringLiteral(
      "(function(){"
      "try{"
      "if(document.querySelector('select[name*=codedeta],select[id*=codedeta]'))return 'codedeta';"
      "return 'other';"
      "}catch(e){return 'error';}})()");
}

QString HeritageIntranetFlow::searchScript() {
  // 확인함(2026-09-11 DOM): 선택다운로드 = searchGisChaRirList('', 'excel'),
  // 전체다운로드 = searchGisChaRirList('', 'excel', 'all').
  // 검색은 같은 함수를 형식 없이 부르는 것이다.
  //
  // 부모 메뉴의 searchGisChaRirList 는 지도 document 를 본다. 그 함수를
  // 집어 오면 시·군이 검색에 안 실린다. 이 창에 codedeta 가 있을 때만
  // 이 창의 함수(또는 폼 안 「검색」)를 쓴다.
  return wrap(QStringLiteral(
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "if(!document.querySelector('select[name*=codedeta],select[id*=codedeta]'))"
      "return 'not-found';"
      "if(typeof W.searchGisChaRirList==='function'){"
      "W.searchGisChaRirList.call(W,'');return 'searching';}"
      "function key(sel){return ((sel.id||'')+' '+(sel.name||'')).toLowerCase();}"
      "var anchor=null;var all=document.querySelectorAll('select');"
      "for(var i=0;i<all.length;i++){"
      "if(key(all[i]).indexOf('codedeta')>=0){anchor=all[i];break;}}"
      "var scope=null;"
      "if(anchor){scope=anchor.form;"
      "if(!scope){var p=anchor;"
      "for(var d=0;d<6&&p;d++){p=p.parentElement;"
      "if(p&&p.querySelectorAll('select').length>=2){scope=p;break;}}}}"
      "function press(root){"
      "var n=root.querySelectorAll('button,input[type=button],input[type=submit],a');"
      "for(var i=0;i<n.length;i++){"
      "if(squash(n[i].innerText||n[i].value)!=='검색')continue;"
      "n[i].click();return true;}"
      "return false;}"
      "if(scope&&press(scope))return 'searching';"
      "return 'not-found';"));
}

QString HeritageIntranetFlow::resultPageInfoScript() {
  // 결과가 여러 쪽일 수 있다. 한 쪽만 받고 끝냈다고 하면 안 된다.
  // 화면의 「검색결과 N건」도 같이 읽는다 — 시/군 조건이 걸렸는지 확인해야 하기 때문이다.
  return wrap(QStringLiteral(
      "var rows=document.querySelectorAll('table tbody tr');"
      "var pages=[];var links=document.querySelectorAll('a');"
      "for(var i=0;i<links.length;i++){"
      "var t=(links[i].innerText||'').trim();"
      "if(/^[0-9]+$/.test(t)){pages.push(parseInt(t,10));}}"
      "var last=pages.length?Math.max.apply(null,pages):1;"
      "var total=-1;"
      "var body=document.body?document.body.innerText:'';"
      "var m=body.match(/검색결과[^0-9]*([0-9,]+)/);"
      "if(m)total=parseInt(m[1].replace(/,/g,''),10);"
      "return JSON.stringify({rows:rows.length,lastPage:last,total:total});"));
}

QString HeritageIntranetFlow::downloadAllScript() {
  // 확인함(2026-09-11 DOM):
  //   전체다운로드 = searchGisChaRirList('', 'excel', 'all')
  //   선택다운로드 = searchGisChaRirList('', 'excel')
  // 버튼을 찾아 누르는 것보다 함수를 직접 부르는 편이 확실하다.
  // 요청이 어떤 형태든(폼 POST·전송 페이지) 브라우저가 받게 두고, 우리는 내려받기를 잡는다.
  return wrap(QStringLiteral(
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "if(!document.querySelector('select[name*=codedeta],select[id*=codedeta]'))"
      "return 'not-found';"
      "if(typeof W.searchGisChaRirList==='function'){"
      "W.searchGisChaRirList.call(W,'','excel','all');return 'all';}"
      "function press(text){var n=document.querySelectorAll('button,input[type=button],input[type=submit],a');"
      "for(var i=0;i<n.length;i++){"
      "if(squash(n[i].innerText||n[i].value)!==text)continue;"
      "n[i].click();return true;}return false;}"
      "if(press('전체다운로드'))return 'all';"
      "var boxes=document.querySelectorAll('table tbody input[type=checkbox]');"
      "if(boxes.length===0)return 'not-found';"
      "for(var i=0;i<boxes.length;i++){if(!boxes[i].checked){boxes[i].click();}}"
      "return press('선택다운로드')?'selected':'not-found';"));
}

QString HeritageIntranetFlow::goToPageScript(int pageNumber) {
  return wrap(QStringLiteral(
                  "var want=String(%1);"
                  "var links=document.querySelectorAll('a');"
                  "for(var i=0;i<links.length;i++){"
                  "if((links[i].innerText||'').trim()===want){links[i].click();return 'moved';}}"
                  "return 'last-page';")
                  .arg(pageNumber));
}

QString HeritageIntranetFlow::frameEscapeScript() {
  // 프레임이 있고 **지금 문서에는 내용이 없을 때만** 안쪽으로 들어간다.
  // 로그인 후 main.do 처럼 프레임이 있어도 본문이 최상위에 있는 화면이 있다.
  return wrap(QStringLiteral(
      "var mine=document.querySelectorAll('input,select,button,a').length;"
      "if(mine>=5)return '';"
      "var fr=document.querySelectorAll('frame,iframe');"
      "if(fr.length===0)return '';"
      "var best='';var bestScore=-1;"
      "for(var i=0;i<fr.length;i++){"
      "var href='';var score=0;"
      "try{var w=fr[i].contentWindow;href=w.location.href;"
      "score=w.document.querySelectorAll('input,select,button,a').length;}catch(e){continue;}"
      "if(!href||href==='about:blank')continue;"
      "if(score>bestScore){bestScore=score;best=href;}}"
      "return bestScore>=5?best:'';"));
}

QString HeritageIntranetFlow::downloadPageProbeScript() {
  // 2026-09-11 실패 outline: 메인 지도에 「시도 선택」(bjdcd)과 메뉴 「전체다운로드」가
  // 있다. 그건 다운로드 화면이 아니다. codedeta 폼이나 서약서만 도착으로 친다.
  return wrap(QStringLiteral(
      "if(document.querySelector('select[name*=codedeta],select[id*=codedeta]'))return 'ready';"
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "var text=squash(document.body?document.body.innerText:'');"
      "if(text.indexOf('서약서에동의')>=0)return 'ready';"
      "return 'not-yet';"));
}

QString HeritageIntranetFlow::pageOutlineScript() {
  // 입력값·비밀번호는 절대 담지 않는다. 요소의 종류와 보이는 글자만 모은다.
  // 막힌 자리에서 다음 선택자를 사람이 보고 정하기 위한 것이다.
  return wrap(QStringLiteral(
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "function texts(sel,cap){var out=[];var n=document.querySelectorAll(sel);"
      "for(var i=0;i<n.length&&out.length<cap;i++){"
      "var t=(n[i].innerText||n[i].value||'').trim();"
      "if(t)out.push(t.slice(0,40));}return out;}"
      "var selects=[];var s=document.querySelectorAll('select');"
      "for(var i=0;i<s.length&&i<16;i++){"
      "selects.push({id:s[i].id||'',name:s[i].name||'',options:s[i].options.length,"
      "first:s[i].options.length?(s[i].options[0].text||'').slice(0,20):'',"
      "picked:s[i].selectedIndex>=0&&s[i].options.length?"
      "(s[i].options[s[i].selectedIndex].text||'').slice(0,20):''});}"
      "var menu=[];var a=document.querySelectorAll('a,button');"
      "for(var i=0;i<a.length&&menu.length<12;i++){"
      "var t=squash(a[i].innerText);"
      "if(t.indexOf('다운로드')<0&&t.indexOf('마이페이지')<0&&t.indexOf('데이터개방')<0)continue;"
      "menu.push({t:t.slice(0,20),href:(a[i].getAttribute('href')||'').slice(0,80),"
      "onclick:(a[i].getAttribute('onclick')||'').slice(0,80)});}"
      "var body=document.body?document.body.innerText:'';"
      "var m=body.match(/검색결과[^0-9]*([0-9,]+)/);"
      "var ifs=[];var fr=document.querySelectorAll('iframe,frame');"
      "for(var i=0;i<fr.length&&i<12;i++){"
      "ifs.push({name:fr[i].name||'',id:fr[i].id||'',"
      "src:(fr[i].getAttribute('src')||'').slice(0,160)});}"
      "return JSON.stringify({"
      "url:W.location.href,title:document.title,"
      "frames:window.frames.length,"
      "htmlFrames:ifs,"
      "codedeta:!!document.querySelector('select[name*=codedeta],select[id*=codedeta]'),"
      "hasShowAgree:!!fn('showAgreePopup'),"
      "hasSearchFn:!!fn('searchGisChaRirList'),"
      "total:m?m[1]:'',"
      "buttons:texts('button,input[type=button],input[type=submit]',20),"
      "links:texts('a',30),"
      "selects:selects,"
      "menu:menu,"
      "checkboxes:document.querySelectorAll('input[type=checkbox]').length,"
      "rows:document.querySelectorAll('table tbody tr').length});"));
}

QString HeritageIntranetFlow::frameInventoryScript() {
  // 자식 document 는 읽지 않는다. 태그에 적힌 name·id·src 만.
  return QStringLiteral(
      "(function(){"
      "try{"
      "var fr=document.querySelectorAll('iframe,frame');"
      "var list=[];"
      "for(var i=0;i<fr.length&&i<16;i++){"
      "list.push({name:fr[i].name||'',id:fr[i].id||'',"
      "src:(fr[i].getAttribute('src')||'').slice(0,200)});}"
      "return JSON.stringify({htmlFrames:fr.length,jsFrames:window.frames.length,list:list});"
      "}catch(e){return 'error:'+String(e);}})()");
}

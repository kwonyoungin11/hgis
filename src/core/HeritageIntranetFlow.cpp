#include "HeritageIntranetFlow.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace {

QString documentHelpers() {
  return QStringLiteral(
      "function visibleElement(e){if(!e)return false;"
      "if(!e.isConnected)return false;"
      "for(var p=e;p&&p.nodeType===1;p=p.parentElement){"
      "var s=p.ownerDocument.defaultView.getComputedStyle(p);"
      "if(s.display==='none'||s.visibility==='hidden'||s.visibility==='collapse'||s.opacity==='0')return false;}"
      "return true;}"
      "function downloadForm(d){var f=d.getElementById('searchForm');"
      "var m=f&&f.querySelector('#mode');return f&&m&&/^[SPBURE]$/.test(m.value)&&visibleElement(f)?f:null;}"
      "function agreementBox(d){var boxes=d.querySelectorAll('input[type=checkbox]');"
      "for(var i=0;i<boxes.length;i++){var b=boxes[i],shown=visibleElement(b);"
      "if(b.labels)for(var j=0;j<b.labels.length;j++)shown=shown||visibleElement(b.labels[j]);"
      "if(!shown)continue;"
      "var t=b.parentElement?b.parentElement.innerText:'';"
      "if(b.labels)for(var j=0;j<b.labels.length;j++)t+=' '+b.labels[j].innerText;"
      "if(t.replace(/\\s+/g,'').indexOf('서약서에동의')>=0)return b;}return null;}"
      "function agreementConfirm(d,b){"
      "for(var p=b.parentElement;p&&p!==d.body;p=p.parentElement){"
      "var nodes=p.querySelectorAll('button,input[type=button],input[type=submit],a'),hits=[];"
      "for(var i=0;i<nodes.length;i++)if(visibleElement(nodes[i])&&"
      "(nodes[i].innerText||nodes[i].value||'').replace(/\\s+/g,'')==='확인')hits.push(nodes[i]);"
      "if(hits.length)return hits.length===1?hits[0]:null;}return null;}");
}


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
             "%2"
             "function hasCodedeta(w){"
             "try{return !!downloadForm(w.document);}"
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
      .arg(body, documentHelpers());
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
  // 빈 문서/로딩 중인 프레임을 로그인 성공으로 취급하지 않는다.
  return wrap(QStringLiteral(
      "var frames=cands();var login=false;"
      "for(var i=0;i<frames.length;i++){var d;try{d=frames[i].document;}catch(e){continue;}"
      "var body=d.body?d.body.innerText:'';"
      "if(body.indexOf('로그아웃')>=0)return 'past-login';"
      "var u=d.getElementById('userId'),p=d.getElementById('pwd');"
      "if(visibleElement(u)&&visibleElement(p))login=true;}"
      "return login?'still-login':'pending';"));
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
      // 「다시 보지 않기」는 **체크상자만** 건드린다. 감싼 요소를 누르면 그 안의 「다음」이 눌려
      // 튜토리얼이 1/5 → 5/5 로 넘어가 버린다(2026-09-12 화면으로 확인).
      "if(again){"
      "var box=again.querySelector('input[type=checkbox]')||"
      "(again.parentElement?again.parentElement.querySelector('input[type=checkbox]'):null);"
      "if(box&&!box.checked){try{box.click();}catch(e){}"
      "if(!box.checked){box.checked=true;"
      "box.dispatchEvent(new Event('change',{bubbles:true}));}}}"
      "if(exit){exit.click();return 'closed';}"
      "return 'closed';"));
}

// 요청 기록으로 확인한 다운로드 화면 주소. 경로를 지어내지 않는다.
QString HeritageIntranetFlow::downloadPagePath() {
  return QStringLiteral("/user/data/heritageDownload.do?pageMenuId=DAT010010");
}

QString HeritageIntranetFlow::downloadListPath() {
  return QStringLiteral("/user/data/heritageDownloadList.do");
}

QString HeritageIntranetFlow::downloadFilesAllPath() {
  return QStringLiteral("/user/data/heritageDownload/downloadFilesAll.do");
}

QString HeritageIntranetFlow::openDownloadPageScript() {
  // 확인함(2026-09-12): 「국가유산자료 다운로드」는 javascript:showAgreePopup(); 이다.
  //
  // **그 주소를 최상위로 직접 열면 안 된다.** heritageDownload.do 는 조각 HTML 이라
  // 통째로 열면 jQuery 도 tabContentAjax 도 없는 반쪽 페이지가 된다(그래서 no-fn 이 계속 났다).
  // 사이트가 제 방식대로 조각을 삽입해야 그 안의 함수들이 생긴다.
  return wrap(QStringLiteral(
      "var c=cands();"
      "for(var i=0;i<c.length;i++){"
      "try{if(downloadForm(c[i].document)||agreementBox(c[i].document))return 'already-open';}catch(e){}}"
      "for(var i=0;i<c.length;i++){"
      "try{if(typeof c[i].showAgreePopup==='function'){c[i].showAgreePopup();return 'opening';}}catch(e){return 'error:'+String(e);}}"
      "return 'not-found';"));
}

QString HeritageIntranetFlow::agreeTermsScript() {
  // 「위 서약서에 동의합니다」 체크 후 「확인」 (2026-09-11 화면 확인).
  // 동의 시각과 서약서 원문은 호출자가 영수증으로 남긴다. 조용히 지나가지 않는다.
  return wrap(QStringLiteral(
      "if(downloadForm(document))return 'already-open';"
      "var target=agreementBox(document);if(!target)return 'not-found';"
      "var confirm=agreementConfirm(document,target);if(!confirm)return 'checked-no-confirm';"
      "var scope=target.parentElement;while(scope&&!scope.contains(confirm))scope=scope.parentElement;"
      "var receipt=scope?(scope.innerText||'').trim():'';if(!receipt)return 'not-found';"
      "if(!target.checked){target.click();"
      "if(!target.checked){target.checked=true;"
      "target.dispatchEvent(new Event('change',{bubbles:true}));}}"
      "confirm.click();return JSON.stringify({status:'agreed',text:receipt});"));
}

QString HeritageIntranetFlow::selectDatasetScript(HeritageDataset dataset) {
  // Use the verified download tabs, not an earlier map-menu LI with the same text.
  const QString code = dataset == HeritageDataset::DesignatedHeritage
      ? QStringLiteral("S") : HeritageStyle::tabCode(dataset);
  // The site's list AJAX replaces #tabContentDiv, including #searchForm.
  // Keep the old node, not its text: two datasets can have the same count.
  const QString rememberForm = QStringLiteral(
      "var oldForm=document.getElementById('searchForm');"
      "if(!oldForm)return 'not-found';"
      "var code=%1;var mode=document.getElementById('mode');"

      "W.__kaHeritagePreviousForm=oldForm;");
  return wrap(rememberForm.arg(jsString(code)) + QStringLiteral("var tab=fn('changeTab');"
                             "if(!tab){return 'no-changeTab';}"
                             "tab(%1); return 'selected';")
                  .arg(jsString(code)));
}

QString HeritageIntranetFlow::datasetReadyScript(HeritageDataset dataset) {
  return wrap(QStringLiteral(
      "var form=document.getElementById('searchForm');"
      "if(!W.__kaHeritagePreviousForm||!form||form===W.__kaHeritagePreviousForm)return 'pending';"
      "var code=%1;var mode=document.getElementById('mode');"
      "if(code&&(!mode||mode.value!==code))return 'pending';"
      "return 'ready';").arg(jsString(dataset == HeritageDataset::DesignatedHeritage ? QStringLiteral("S") : HeritageStyle::tabCode(dataset))));
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
                  "var df=downloadForm(document);"
                  "var all=Array.prototype.slice.call((df||document).querySelectorAll('select'));"
                  "var sidoAt=-1;"
                  // 1순위: 다운로드 폼의 이름(codedeta). 지도 패널(bjdcd)은 제외한다.
                  "for(var i=0;i<all.length;i++){"
                  "if(key(all[i]).indexOf('codedeta')>=0&&firstOption(all[i]).indexOf('시도선택')>=0){sidoAt=i;break;}}"
                  "if(sidoAt<0){for(var i=0;i<all.length;i++){"
                  "if(!df&&key(all[i]).indexOf('bjdcd')>=0)continue;"
                  "var f=firstOption(all[i]);""if(f.indexOf('시도선택')>=0||f.indexOf('시·도선택')>=0||f.indexOf('시/도선택')>=0){sidoAt=i;break;}}}"
                  "if(sidoAt<0)return 'not-found';"
                  "var sidoSel=all[sidoAt];"
                  // 시/군/구는 같은 폼 안에서 시/도 바로 뒤에 오는 select 다.
                  "var citySel=null;"
                  "for(var i=sidoAt+1;i<all.length;i++){"
                  "if(sidoSel.form&&all[i].form!==sidoSel.form)continue;"
                  "var f=firstOption(all[i]);"
                  "if(f.indexOf('시/군/구선택')>=0||f.indexOf('시군구선택')>=0||f.indexOf('시·군·구선택')>=0){citySel=all[i];break;}}"
                  "function pick(sel,text,exact){if(!sel)return false;"
                  "for(var i=0;i<sel.options.length;i++){"
                  "if(exact?squash(sel.options[i].text)!==squash(text):!same(sel.options[i].text,text))continue;"
                  "if(sel.selectedIndex!==i){sel.selectedIndex=i;"
                  "sel.dispatchEvent(new Event('input',{bubbles:true}));"
                  "sel.dispatchEvent(new Event('change',{bubbles:true}));}"
                  "return true;}"
                  "return false;}"
                  "if(!pick(sidoSel,%1))return 'no-sido';"
                  "if(!citySel)return 'sigungu-pending';"
                  "if(pick(citySel,%2,true))return 'selected';"
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
      "(function(){%1"
      "try{"
      "if(downloadForm(document))return 'codedeta';"
      "return 'other';"
      "}catch(e){return 'error';}})()").arg(documentHelpers());
}

QString HeritageIntranetFlow::searchScript() {
  // **페이지를 옮기지 않는다.** (검사 noScriptMayNavigateThePage 가 지킨다)
  //
  // 왜 계속 「검색을 못 찾았다」였나:
  // codedeta 칸에서 위로 올라가다 select 가 2개인 첫 상자에서 멈췄는데,
  // 화면상 「검색」 버튼은 그 상자 **바깥 옆**에 있다. 그래서 영영 못 찾았다.
  //
  // 사이트 JS 가 폼 이름을 알려 준다: $("#searchForm").serialize()
  // 그러니 범위는 #searchForm 이다. 그게 없으면
  // **codedeta 칸과 「검색」 버튼을 함께 품은 가장 가까운 조상**까지 올라간다.
  // 왼쪽 지도 패널의 「검색」을 누르면 조건이 안 걸린 채 전국이 검색되므로 범위는 반드시 좁힌다.
  return wrap(QStringLiteral(
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "function key(e){return ((e.id||'')+' '+(e.name||'')).toLowerCase();}"
      "function findSearch(root){"
      "if(!root||!root.querySelectorAll)return null;"
      "var n=root.querySelectorAll('button,input[type=button],input[type=submit],a');"
      "for(var i=0;i<n.length;i++){"
      "if(squash(n[i].innerText||n[i].value)==='검색')return n[i];}"
      "return null;}"
      "var c=cands();"
      "for(var w=0;w<c.length;w++){"
      "var doc;try{doc=c[w].document;}catch(e){continue;}"
      "var df=downloadForm(doc);var anchor=df?df.querySelector('select'):null;var sels=doc.querySelectorAll('select');"
      "for(var i=0;!anchor&&i<sels.length;i++){"
      "if(key(sels[i]).indexOf('codedeta')>=0){anchor=sels[i];break;}}"
      "if(!anchor)continue;"
      // ① 사이트가 부르는 이름 그대로
      "var scope=doc.getElementById('searchForm');"
      "var btn=scope?findSearch(scope):null;"
      // ② 없으면 codedeta 와 「검색」을 함께 품은 가장 가까운 조상까지 올라간다
      "if(!btn){var p=anchor;"
      "for(var d=0;d<10&&p;d++){p=p.parentElement;if(!p)break;"
      "var found=findSearch(p);"
      "if(found){btn=found;break;}}}"
      "if(btn){c[w].__kaHeritageSearchNode=doc.querySelector('#tabContentDiv table tbody')||scope;btn.click();return 'searching';}}"
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
      "var previous=W.__kaHeritageSearchNode;"
      "var searchReady=!!previous&&!previous.isConnected;"
      "return JSON.stringify({rows:rows.length,lastPage:last,total:total,searchReady:searchReady});"));
}

QString HeritageIntranetFlow::downloadAllScript() {
  // 검색한 다운로드 패널의 실제 버튼을 한 번 누른다. 같은 이름의 지도 함수를
  // 직접 호출하거나 별도 URL을 만들지 않고 버튼에 연결된 사이트 동작을 사용한다.
  return wrap(QStringLiteral(
      "function squash(t){return (t||'').replace(/\\s+/g,'');}"
      "var form=downloadForm(document);if(!form)return 'not-found';"
      "var sido=form.querySelector('select[name=codedetaCd],select[name=bjdCd]');"
      "var sgg=form.querySelector('select[name=codeCdSg],select[name=bjdCd1]');"
      "if(!sido||!sido.value)return 'no-region';"
      "if(!sgg||!sgg.value)return 'no-region';"
      "var panel=form.closest('#tabContentDiv');if(!panel)return 'not-found';"
      "var n=panel.querySelectorAll('button,input[type=button],input[type=submit],a'),hits=[];"
      "for(var i=0;i<n.length;i++)if(visibleElement(n[i])&&!n[i].disabled&&"
      "n[i].getAttribute('aria-disabled')!=='true'&&"
      "squash(n[i].innerText||n[i].value)==='전체다운로드')hits.push(n[i]);"
      "if(!hits.length)return 'not-found';"
      "if(hits.length!==1)return 'ambiguous';"
      // DOM click 중 발생한 이벤트 핸들러 예외는 click()의 try/catch로 잡히지 않는다.
      // 잠깐 error 이벤트를 관찰하되 계정/요청 값이 섞일 수 있는 오류 본문은 반환하지 않는다.
      "var error='';var host=document.defaultView;"
      "function onError(e){error=(e.error&&e.error.name)||'JavaScriptError';}"
      "host.addEventListener('error',onError);"
      "try{hits[0].click();}catch(e){error=e.name||'JavaScriptError';}"
      "finally{host.removeEventListener('error',onError);}"
      "return error?('error:'+error):'all';"));
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
      "var body=document.body?document.body.innerText:'';"
      "if(/찾을\\s*수\\s*없는\\s*페이지/.test(body))return 'page-error';"
      "if(downloadForm(document)||agreementBox(document))return 'ready';"
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
      // 주소는 읽기만 한다. 대입하지 않는다(페이지를 옮기지 않는다).
      "url:String(W.location.href),title:document.title,"
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

QString HeritageIntranetFlow::redactRequestLine(const QString& method, const QUrl& url) {
  const QString path = url.toString(QUrl::RemoveQuery);
  const QString lower = path.toLower();
  // 로그인 계열은 주소도 질의도 남기지 않는다. 비밀번호가 실려 간다.
  if (lower.contains(QLatin1String("security_check")) || lower.contains(QLatin1String("login")) ||
      lower.contains(QLatin1String("checknet")))
    return method + QStringLiteral(" <로그인 요청 — 내용을 남기지 않음>");
  QString line = method + QLatin1Char(' ') + path;
  const QString query = url.query();
  if (!query.isEmpty()) line += QLatin1Char('?') + query.left(400);
  return line;
}

QString HeritageIntranetFlow::loadListScript(const QString& params) {
  // 사이트 자신의 전송 통로를 쓴다. 서버가 기대하는 맥락(헤더·세션·화면 상태)이 그대로 붙는다.
  // tabContentAjax 가 어느 창에 있는지는 화면마다 다르다. **함수가 있는 창을 찾아 그 창에서** 부른다.
  // 함수만 꺼내 부르면 그 안의 $("#tabContentDiv") 가 엉뚱한 문서를 가리킨다.
  return wrap(QStringLiteral("var c=cands();var host=null;"
                             "for(var i=0;i<c.length;i++){"
                             "try{if(typeof c[i].tabContentAjax==='function'){host=c[i];break;}}catch(e){}}"
                             "if(!host)return 'no-fn';"
                             "host.tabContentAjax(%1,%2);"
                             "return 'sent';")
                  .arg(jsString(downloadListPath()), jsString(params)));
}

QString HeritageIntranetFlow::tabContentHtmlScript() {
  // 결과는 tabContentAjax 를 가진 창의 #tabContentDiv 에 들어간다. 그 창에서 읽는다.
  return wrap(QStringLiteral("var c=cands();"
                             "for(var i=0;i<c.length;i++){"
                             "try{var d=c[i].document.getElementById('tabContentDiv');"
                             "if(d&&d.innerHTML&&d.innerHTML.length>200)return d.innerHTML;}catch(e){}}"
                             "return '';"));
}

QString HeritageIntranetFlow::buildDownloadUrlScript(const QString& tabCode) {
  // 사이트 함수를 부를 수 없다: 지도(js/ngis/bury/select.js)가 같은 이름의
  // 「군부대유산조사」용 searchGisChaRirList 를 나중에 정의해 덮어쓴다.
  // 그 함수는 #searchHeName 을 읽어 null.value 로 터진다(2026-09-13 스택으로 확인).
  //
  // 그래서 값을 **날것 그대로** JSON 으로 넘긴다. 인코딩은 C++(QUrlQuery)이 한 번만 한다.
  // JS 에서 인코딩하면 Qt 가 또 해서 %25 가 된다(2026-09-12 두 번 당했다).
  return wrap(QStringLiteral(
                  "var c=cands();var doc=null;"
                  "for(var w=0;w<c.length;w++){"
                  "var d;try{d=c[w].document;}catch(e){continue;}"
                  "if(d.getElementById('searchForm')||d.getElementById('codedetaCd0')){doc=d;break;}}"
                  "if(!doc)return 'not-found';"
                  "var df=downloadForm(doc);"
                  "function regionSel(kind){var all=(df||doc).querySelectorAll('select');"
                  "for(var i=0;i<all.length;i++){"
                  "var k=((all[i].id||'')+' '+(all[i].name||'')).toLowerCase();"
                  "if(k.indexOf('bjdcd')>=0)continue;"
                  "if(k.indexOf(kind)>=0)return all[i];}"
                  "return null;}"
                  "var sido=(df?df.querySelector('select[name=codedetaCd],select[name=bjdCd]'):null)||doc.getElementById('codedetaCd0')||regionSel('codedeta');"
                  "var sgg=(df?df.querySelector('select[name=codeCdSg],select[name=bjdCd1]'):null)||doc.getElementById('codeCdSg0')||regionSel('codecdsg');"
                  "if(!sido||!sido.value)return 'no-region';"
                  "if(!sgg||!sgg.value)return 'no-region';"
                  "var form=doc.getElementById('searchForm');"
                  "var scope=form||doc;"
                  "var out=[];"
                  "var els=scope.querySelectorAll('input,select,textarea');"
                  "for(var i=0;i<els.length;i++){var e=els[i];"
                  "if(!e.name)continue;"
                  "var t=(e.type||'').toLowerCase();"
                  "if(t==='submit'||t==='button')continue;"
                  "if((t==='checkbox'||t==='radio')&&!e.checked)continue;"
                  "out.push([e.name,e.value==null?'':String(e.value)]);}"
                  "var p=String(sido.value).split(',');"
                  "if(sido.name==='codedetaCd')out.push(['codeCd',p.length>1?p[1]:'']);"
                  "var modeEl=doc.getElementById('mode');"
                  "var tab=(modeEl&&modeEl.value)?modeEl.value:%1;"
                  "out.push(['tab',tab]);"
                  "if(!modeEl)out.push(['mode',tab]);"
                  // searchParams 는 화면에 보여 줄 요약. 날것으로 만든다(인코딩하지 않는다).
                  "function picked(el){if(!el||el.selectedIndex<0||!el.options.length)return '';"
                  "return (el.options[el.selectedIndex].text||'').trim();}"
                  "var a=picked(sido);var b=picked(sgg);"
                  "out.push(['searchParams',a?('지역:'+a+(b?' '+b:'')+' / '):'']);"
                  "return JSON.stringify({path:%2,params:out});")
                  .arg(jsString(tabCode), jsString(downloadFilesAllPath())));
}

QString HeritageIntranetFlow::verifyRegionScript() {
  // 탭을 바꾸면 폼이 ajax 로 다시 그려지고 시/군 목록은 spcaCdtSiGunGu.do 가 따로 채운다.
  // 그래서 「골랐다」를 한 번 보고 믿으면 안 된다(2026-09-12: 1초 만에 selected 를 받았지만
  // 실제로는 값이 비어 전국을 요청했다). 지금 값이 남아 있는지 다시 본다.
  // 반환: "ok:<시도값>|<시군구값>" 또는 "empty"
  return wrap(QStringLiteral(
      "var df=downloadForm(document);"
      "function pickSel(kind){var all=(df||document).querySelectorAll('select');"
      "for(var i=0;i<all.length;i++){"
      "var k=((all[i].id||'')+' '+(all[i].name||'')).toLowerCase();"
      "if(k.indexOf('bjdcd')>=0)continue;"
      "if(k.indexOf(kind)>=0)return all[i];}"
      "return null;}"
      "var a=(df?df.querySelector('select[name=codedetaCd],select[name=bjdCd]'):null)||pickSel('codedeta');"
      "var b=(df?df.querySelector('select[name=codeCdSg],select[name=bjdCd1]'):null)||pickSel('codecdsg');"
      "if(!a||!a.value)return 'empty';"
      "if(!b||!b.value)return 'empty';"
      "return 'ok:'+a.value+'|'+(b?b.value:'');"));
}

const isNodeJs = (typeof window) === 'undefined'? true : false;

if　(isNodeJs)　{
  var Benchmark = require('benchmark');
  var cv = require('../../opencv');
  var HelpFunc = require('../perf_helpfunc');
  var Base = require('../base');
} else {
  var paramsElement = document.getElementById('params');
  var runButton = document.getElementById('runButton');
  var logElement = document.getElementById('log');
}

function perf() {

    console.log('opencv.js loaded');
    if (isNodeJs) {
      global.cv = cv;
      global.combine = HelpFunc.combine;
      global.cvtStr2cvSize = HelpFunc.cvtStr2cvSize;
      global.cvSize = Base.getCvSize();
    } else {
      enableButton();
      cvSize = getCvSize();
    }
    let totalCaseNum, currentCaseId;

    const SobelSize = [cvSize.szODD, cvSize.szQVGA, cvSize.szVGA];
    const Sobel3x3dxdy = ["(0,1)", "(1,0)", "(1,1)", "(0,2)", "(2,0)", "(2,2)"];
    const Sobeldxdy = ["(0,1)", "(1,0)", "(1,1)", "(0,2)", "(2,0)"];
    const BorderType3x3 = ["BORDER_REPLICATE", "BORDER_CONSTANT"];
    const BorderType3x3ROI = ["BORDER_DEFAULT", "BORDER_REPLICATE|BORDER_ISOLATED", "BORDER_CONSTANT|BORDER_ISOLATED"];
    const BorderType = ["BORDER_REPLICATE", "BORDER_CONSTANT", "BORDER_REFLECT", "BORDER_REFLECT101"];
    const BorderTypeROI = ["BORDER_DEFAULT", "BORDER_REPLICATE|BORDER_ISOLATED", "BORDER_CONSTANT|BORDER_ISOLATED", "BORDER_REFLECT|BORDER_ISOLATED", "BORDER_REFLECT101|BORDER_ISOLATED"]

    const combiSobelBorder3x3 = combine(SobelSize, ["CV_16SC1", "CV_32FC1"], Sobel3x3dxdy, BorderType3x3);
    const combiSobelBorder3x3ROI = combine(SobelSize, ["CV_16SC1", "CV_32FC1"], Sobel3x3dxdy, BorderType3x3ROI);
    const combiSobelBorder5x5 = combine(SobelSize, ["CV_16SC1", "CV_32FC1"], Sobeldxdy, BorderType);
    const combiSobelBorder5x5ROI = combine(SobelSize, ["CV_16SC1", "CV_32FC1"], Sobeldxdy, BorderTypeROI);

    function addSobelCase(suite, type) {
        suite.add('sobel', function() {
            cv.Sobel(src, dst, ddepth, dx, dy, ksize, 1, 0, borderType);
          }, {
              'setup': function() {
                let size = this.params.size;
                let ddepth = cv[this.params.ddepth];
                let dxdy = this.params.dxdy;
                let ksize = this.params.ksize;
                
                let type = this.params.type;
                let src, dst;
                
                if (type %2 == 0) {
                  src = new cv.Mat(size[1], size[0], cv.CV_8U);
                  dst = new cv.Mat(size[1], size[0], ddepth);
                } else {
                  src = new cv.Mat(size[1]+10, size[0]+10, cv.CV_8U);
                  dst = new cv.Mat(size[1]+10, size[0]+10, ddepth);
                  src = src.colRange(5, size[0]+5);
                  src = src.rowRange(5, size[1]+5);
                  dst = dst.colRange(5, size[0]+5);
                  dst = dst.rowRange(5, size[1]+5);
                }

                let dx = parseInt(dxdy[1]);
                let dy = parseInt(dxdy[3]);
                let borderTypeArray = this.params.borderType;
                let borderType;
                if (borderTypeArray.length == 1) {
                  borderType = cv[borderTypeArray[0]];
                } else {
                  borderType = cv[borderTypeArray[0]] | cv[borderTypeArray[1]];
                }
                },
              'teardown': function() {
                src.delete();
                dst.delete();
              }
          });
    }

    function addSobelModeCase(suite, combination, type) {
      totalCaseNum += combination.length;
      for (let i = 0; i < combination.length; ++i) {
        let size =  combination[i][0];
        let ddepth = combination[i][1];
        let dxdy = combination[i][2];
        let borderType = combination[i][3];
        let sizeArray = [size.width, size.height];
        let ksize;
        if (type < 2) {
          ksize = 3;
        } else {
          ksize = 5;
        }

        let borderTypeArray = borderType.split("|");
        let params = {size: sizeArray, ddepth: ddepth, dxdy: dxdy, ksize:ksize, borderType:borderTypeArray, type:type};
        addKernelCase(suite, params, type, addSobelCase);
      }
    }

    function genBenchmarkCase(paramsContent) {
        let suite = new Benchmark.Suite;
        totalCaseNum = 0;
        currentCaseId = 0;
        let params = "";
        let paramObjs = [];
        paramObjs.push({name:"size", value:"", reg:[""], index:0});
        paramObjs.push({name:"ddepth", value:"", reg:["/CV\_[0-9]+[FSUfsu]C1/g"], index:1});
        paramObjs.push({name:"dxdy", value:"", reg:["/\\([0-2],[0-2]\\)/"], index:2});

        if (/\([0-9]+x[0-9]+,[\ ]*\w+,[\ ]*\([0-2],[0-2]\),[\ ]*\w+\)/g.test(paramsContent.toString())) {
            params = paramsContent.toString().match(/\([0-9]+x[0-9]+,[\ ]*\w+,[\ ]*\([0-2],[0-2]\),[\ ]*\w+\)/g)[0];
            paramObjs.push({name:"boderType", value:"", reg:["/BORDER\_\\w+/"], index:3});
        } else if (/\([0-9]+x[0-9]+,[\ ]*\w+,[\ ]*\([0-2],[0-2]\),[\ ]*\w+\|\w+\)/g.test(paramsContent.toString())) {
            params = paramsContent.toString().match(/\([0-9]+x[0-9]+,[\ ]*\w+,[\ ]*\([0-2],[0-2]\),[\ ]*\w+\|\w+\)/g)[0];
            paramObjs.push({name:"boderType", value:"", reg:["/BORDER\_\\w+\\|BORDER\_\\w+/"], index:3});
        }
        
        if (params != ""){
            let locationList = decodeParams2Case(params, paramObjs,sobelCombinations);
            for (let i = 0; i < locationList.length; i++){
                let first = locationList[i][0];
                let second = locationList[i][1];
                addSobelModeCase(suite, [sobelCombinations[first][second]], first);
              }
        } else {
          log("no filter or getting invalid params, run all the cases");
          addSobelModeCase(suite, combiSobelBorder3x3, 0);
          addSobelModeCase(suite, combiSobelBorder3x3ROI, 1);
          addSobelModeCase(suite, combiSobelBorder5x5, 2);
          addSobelModeCase(suite, combiSobelBorder5x5ROI, 3);
        }
        setBenchmarkSuite(suite, "sobel", currentCaseId);
        log(`Running ${totalCaseNum} tests from Sobel`);
        suite.run({ 'async': true }); // run the benchmark
    }

    let sobelCombinations = [combiSobelBorder3x3, combiSobelBorder3x3ROI, combiSobelBorder5x5, combiSobelBorder5x5ROI];

    if (isNodeJs) {
        const args = process.argv.slice(2);
        let paramsContent = '';
        if (/--test_param_filter=\([0-9]+x[0-9]+,[\ ]*\w+,[\ ]*\([0-2],[0-2]\),[\ ]*\w+\)/g.test(args.toString())) {
          paramsContent = args.toString().match(/\([0-9]+x[0-9]+,[\ ]*\w+,[\ ]*\([0-2],[0-2]\),[\ ]*\w+\)/g)[0];
        } else if (/--test_param_filter=\([0-9]+x[0-9]+,[\ ]*\w+,[\ ]*\([0-2],[0-2]\),[\ ]*\w+\|\w+\)/g.test(args.toString())) {
          paramsContent = args.toString().match(/\([0-9]+x[0-9]+,[\ ]*\w+,[\ ]*\([0-2],[0-2]\),[\ ]*\w+\|\w+\)/g)[0];
        }
        genBenchmarkCase(paramsContent);
      } else {
        runButton.onclick = function()　{
          let paramsContent = paramsElement.value;
          genBenchmarkCase(paramsContent);
          if (totalCaseNum !== 0) {
            disableButton();
          }
        }
      }
};

async function main() {
  if (cv instanceof Promise) {
    cv = await cv;
    perf();
  } else {
    cv.onRuntimeInitialized = perf;
  }
}

main();